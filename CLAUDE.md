# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

STM32F103RC + FreeRTOS v10.3.1 双电机速度闭环控制系统（两轮差分驱动）。详见 [README.md](README.md)。

## 构建命令

```bash
# 使用 Keil MDK-ARM 命令行编译
UV4 -b MDK-ARM/7.6.uvprojx -o build.log

# 或者在 Keil IDE 中打开项目文件直接编译下载
# 项目文件: MDK-ARM/7.6.uvprojx
```

- 编译器: ARM Compiler 5 (armcc)
- 目标芯片: STM32F103RC
- **使用 MicroLib**（Keil 勾选 Use MicroLib），printf 通过 `fputc` 重定向到 `HAL_UART_Transmit`

## 硬件架构

| 外设 | 定时器 | 引脚 | 功能 |
|------|--------|------|------|
| PWM（电机A） | TIM3 CH1/CH2 | PA6/PA7 | 左电机正反转 |
| PWM（电机B） | TIM3 CH3/CH4 | PB0/PB1 | 右电机正反转 |
| 编码器A | TIM4 | PB6(CH1)/PB7(CH2) | 左电机 AB 相输入 |
| 编码器B | TIM8 | PC6(CH1)/PC7(CH2) | 右电机 AB 相输入 |
| 串口 | USART1 | PA9(TX)/PA10(RX) | 调试通信 115200-8N1 |

### 时钟配置

- HSE 外部晶振 → PLL ×9 → SYSCLK = 72MHz
- APB1 = 36MHz, APB2 = 72MHz
- SysTick 用于 FreeRTOS 时基 (1ms tick)
- TIM7 用于 HAL 时基

### PWM 参数

- TIM3 时钟 72MHz, Prescaler=0, Period=65535
- PWM 频率 ≈ 1098Hz (72MHz / 65536)
- 占空比分辨率: 65535 级 (16-bit)
- 正转: CH1/CH3 输出 PWM, CH2/CH4 输出 0
- 反转: 相反

### 编码器参数

- 编码器规格: 2000 PPR（物理线数），×4 倍频后 = **8000 计数/转**（电机轴）
- 编码器模式: ×4 倍频 (`TIM_ENCODERMODE_TI12`)
- 电机带减速箱，**减速比 ≈ 7.5:1**（待确认精确值：输出轴转一圈显示约 15000 脉冲 → 60000 计数 → 60000/8000 = 7.5 圈电机轴）
- 16 位计数器，溢出通过 `(int32_t)(int16_t)(raw - last)` 自动处理
- 编码器引脚配置为 `GPIO_MODE_INPUT`, `GPIO_PULLUP`（PA10 上拉）
- **A 轮编码器疑似未连接**（始终为 0），B 轮编码器正常

## FreeRTOS 任务架构

| 任务 | 优先级 | 周期 | 职责 |
|------|--------|------|------|
| `EncoderTask` | Low | 1ms | `int16_t` 读取 TIM4/TIM8 CNT → `encoder_count_A/B` |
| `ControlSpeedTas` | Normal | 10ms | 算速度(mm/s) + PID 计算 → `motor_pwm_A/B` |
| `ControlMotorTas` | Normal | 1ms | `motor_pwm` → TIM3 比较值 + 正反转逻辑 |
| `defaultTask` | Normal | 1000ms | 启动外设 + printf 状态输出 |

数据流:
```
EncoderTask → encoder_count → ControlSpeedTas(PID) → motor_pwm → ControlMotorTas(PWM)
```

优先级注意: EncoderTask 用 Low 保证先被抢占（数据生产者），ControlSpeedTas 和 ControlMotorTas 用 Normal。

### FreeRTOS 配置要点

- `configTICK_RATE_HZ = 1000` (1ms)
- `configTOTAL_HEAP_SIZE = 3072` (heap_4.c)
- CMSIS-RTOS V2 封装层 (`cmsis_os2.c`)

## 代码约定

### STM32CubeMX 生成代码的边界

所有用户代码必须写在 `/* USER CODE BEGIN ... */` 和 `/* USER CODE END ... */` 之间。CubeMX 重新生成时会覆盖边界外的代码。

### 核心文件

| 文件 | 内容 |
|------|------|
| `Core/Src/main.c` | 入口, 时钟配置 (HSE+PLL=72MHz), 外设初始化, 启动调度器 |
| `Core/Src/freertos.c` | **主要应用逻辑** — 全局变量、PID 结构体、4 个任务、PID 函数 |
| `Core/Src/tim.c` | TIM3/4/8 初始化, GPIO 引脚配置 (MSP) |
| `Core/Src/usart.c` | USART1 初始化 + `fputc` 重定向到 `HAL_UART_Transmit` |
| `Core/Inc/main.h` | 引脚宏定义 (`PWMA_IN1_Pin` 等) |
| `Core/Inc/FreeRTOSConfig.h` | FreeRTOS 内核配置 |

### 关键变量（freertos.c 全局）

```c
volatile int32_t encoder_count_A, encoder_count_B;  // 硬件CNT瞬时值（int16_t范围）
volatile int32_t encoder_accum_A, encoder_accum_B;  // 编码器累计脉冲（4倍频值）
volatile float motor_pwm_A, motor_pwm_B;             // PID 输出 [-100, 100]
volatile float speed_target_A, speed_target_B;       // 目标速度 mm/s
volatile float current_speed_A, current_speed_B;     // 当前速度 mm/s
PID_HandleTypeDef pid_A, pid_B;                      // PID 参数
```

### 物理参数宏（freertos.c）

```c
#define ENCODER_PPR_PHYSICAL     2000        // 编码器物理线数
#define ENCODER_QUADRATURE       4           // 4倍频
#define ENCODER_PPR              (ENCODER_PPR_PHYSICAL * ENCODER_QUADRATURE) // 8000
#define WHEEL_DIAMETER_MM        70.0f       // 轮子直径 mm（需实际测量后修改）
#define WHEEL_CIRCUMFERENCE_MM   (3.1415926f * WHEEL_DIAMETER_MM)
#define SPEED_DT                 0.01f       // 速度测量周期 10ms
```

### 速度换算公式

```
speed(mm/s) = (delta / 0.01s) / 8000 * (π × 70mm)
```
- delta: 10ms 内 4倍频计数增量
- 电机轴转一圈 = 8000 计数 → speed = 8000/0.01/8000 * π*70 ≈ 220 mm/s

### PID 相关

- 当前: **位置式 PID** — `u = Kp*e + Ki*∫e + Kd*de/dt`
- 目标: **增量式 PI** — `Δu = Kp*(e[k]-e[k-1]) + Ki*e[k]`, `u += Δu`
- 输出限幅: [-100, 100] 对应占空比百分比

## 串口输出

- MicroLib + `fputc` 重定向到 `HAL_UART_Transmit(&huart1, ...)`
- `UART_MODE_TX` 仅发送，无 RX 中断
- 波特率: 115200-8N1
- printf 每秒输出: `A_Speed:mm/s Pulse:累计脉冲 | B_Speed:mm/s Pulse:累计脉冲`

## PID 控制

- 当前: **增量式 PI** — `Δu = Kp*(e[k]-e[k-1]) + Ki*e[k]`, `u += Δu`
- Kd 保留在结构体中但未使用
- 输出限幅: [-100, 100] 对应占空比百分比

## 已知待完成事项

1. **减速比确认**: 实测输出轴转一圈显示约 15000 脉冲（预期 2000），减速比约 7.5:1，需确认精确值并加入 ENCODER_PPR 计算
2. **编码器校准**: 将减速比纳入速度公式，使显示值和实际速度一致
3. **A 轮编码器**: 排查 A 轮编码器接线（始终为 0，B 轮正常）
4. **串口接收**: 后续可添加 UART RX 中断 + 环形缓冲区 + 串口命令解析
5. **数据记录**: CSV 格式输出速度响应曲线