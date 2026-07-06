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
- **使用标准库，非 Microlib**（通过 `startup_stm32f103xe.lst` 确认 `__MICROLIB` 未定义）

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

- ×4 倍频模式 (`TIM_ENCODERMODE_TI12`)，500 线编码器 → 2000 脉冲/转
- 16 位计数器，溢出通过 `(int32_t)(int16_t)(raw - last)` 自动处理
- 编码器引脚配置为 `GPIO_MODE_INPUT`, `GPIO_NOPULL`

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
volatile int32_t encoder_count_A, encoder_count_B;  // 编码器原始值
volatile float motor_pwm_A, motor_pwm_B;             // PID 输出 [-100, 100]
volatile float speed_target_A, speed_target_B;       // 目标速度 mm/s
volatile float current_speed_A, current_speed_B;     // 当前速度 mm/s
PID_HandleTypeDef pid_A, pid_B;                      // PID 参数
```

### 物理参数宏（freertos.c）

```c
#define ENCODER_PPR              2000        // 编码器脉冲/转（4倍频后理论值）
#define WHEEL_DIAMETER_MM        65.0f       // 轮子直径 mm（需实际测量后修改）
#define WHEEL_CIRCUMFERENCE_MM   (3.1415926f * WHEEL_DIAMETER_MM)
#define SPEED_DT                 0.01f       // 速度测量周期 10ms
```

### 速度换算公式

```
speed(mm/s) = (delta / 0.01s) / 2000 * (π × 65mm)
```

### PID 相关

- 当前: **位置式 PID** — `u = Kp*e + Ki*∫e + Kd*de/dt`
- 目标: **增量式 PI** — `Δu = Kp*(e[k]-e[k-1]) + Ki*e[k]`, `u += Δu`
- 输出限幅: [-100, 100] 对应占空比百分比

## 串口输出

- fputc 重定向到 `HAL_UART_Transmit(&huart1, ...)`
- 使用标准库（非 Microlib），printf 调用链: `printf → __2printf → _printf_char_file → fputc → HAL_UART_Transmit`
- 当前只有 TX 输出，无 RX 中断接收
- 波特率: 115200-8N1

## 已知待完成事项

按优先级排列:
1. **串口修复**: 添加 UART RX 中断 + 环形缓冲区 + `_sys_write`（确保 printf 可靠性）
2. **开环控制**: `control_mode` 变量 + `openloop_pwm_A/B` + 串口命令解析
3. **增量式 PI**: 替换位置式 PID_Calculate()，PI 只用比例积分
4. **数据记录**: CSV 格式输出速度响应曲线，支持 START_LOG/STOP_LOG