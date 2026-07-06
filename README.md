# ByCarAssignment - STM32F103 两轮差分驱动电机速度闭环控制系统

## 项目概述

### 项目名称
ByCarAssignment - 基于STM32F103的两轮差分驱动电机速度闭环控制系统

### 简介
本项目是一个基于STM32F103RC微控制器和FreeRTOS实时操作系统的两轮差分驱动电机速度闭环控制系统。系统采用PID控制器实现精确的速度控制，通过编码器反馈实现闭环控制，支持串口通信进行调试和监控。

### 主要功能

- **编码器读取**：双通道编码器数据采集（TIM4/TIM8）
- **PID速度控制**：双路独立PID控制器实现精确速度调节
- **PWM电机驱动**：4通道PWM输出控制左右电机正反转
- **串口通信**：115200bps串口调试输出，实时监控速度状态
- **多任务调度**：基于FreeRTOS的实时任务调度，保证控制实时性

---

## 环境要求

### 硬件要求
| 组件 | 规格 | 说明 |
|------|------|------|
| MCU | STM32F103RC | ARM Cortex-M3, 72MHz, 64KB RAM, 256KB Flash |
| 电机驱动板 | L298N/L293D | 双路H桥电机驱动 |
| 编码器 | 增量式编码器 | AB相输出，建议1000线以上 |
| 电源 | 12V/5V | 电机供电12V，单片机供电5V |

### 软件要求
| 工具 | 版本 | 用途 |
|------|------|------|
| Keil MDK-ARM | v5.36+ | 集成开发环境 |
| STM32CubeMX | v6.8+ | 项目配置工具 |
| Python | v3.8+ | 串口调试（可选） |
| 串口助手 | 任意 | 监控调试输出 |

---

## 安装与配置

### 步骤1：克隆项目

```bash
git clone https://github.com/keyuan1234/ByCarAssignment.git
cd ByCarAssignment
git checkout 7.6
```

### 步骤2：打开项目

1. 启动 Keil MDK-ARM
2. 打开 `MDK-ARM/7.6.uvprojx` 项目文件
3. 等待项目加载完成

### 步骤3：配置编译环境

1. 确保已安装 STM32F1xx 系列芯片支持包
2. 配置编译器路径（默认使用 ARM Compiler 5/6）
3. 确认目标设备为 `STM32F103RC`

### 步骤4：编译项目

```bash
# 在Keil中执行编译
# 或使用命令行（需配置UV4路径）
UV4 -b MDK-ARM/7.6.uvprojx -o build.log
```

### 步骤5：下载到硬件

1. 连接STM32开发板到电脑
2. 配置调试器（ST-Link/V2）
3. 在Keil中点击"Download"按钮下载固件

---

## 使用方法

### 硬件连接

| STM32引脚 | 连接目标 | 功能 |
|-----------|----------|------|
| PA6 | 左电机PWM1 | TIM3_CH1 - PWM输出 |
| PA7 | 左电机PWM2 | TIM3_CH2 - PWM输出 |
| PB0 | 右电机PWM1 | TIM3_CH3 - PWM输出 |
| PB1 | 右电机PWM2 | TIM3_CH4 - PWM输出 |
| PB6 | 左编码器A相 | TIM4_CH1 - 编码器输入 |
| PB7 | 左编码器B相 | TIM4_CH2 - 编码器输入 |
| PC6 | 右编码器A相 | TIM8_CH1 - 编码器输入 |
| PC7 | 右编码器B相 | TIM8_CH2 - 编码器输入 |
| PA9 | USB-TX | USART1_TX - 串口输出 |
| PA10 | USB-RX | USART1_RX - 串口输入 |

### 串口通信

系统启动后，通过串口工具连接（115200bps, 8N1），可看到以下输出：

```
System initialized
Speed A: 0.0, Target A: 0.0, Speed B: 0.0, Target B: 0.0
Speed A: 125.3, Target A: 100.0, Speed B: 123.8, Target B: 100.0
```

### 设置目标速度

修改 `freertos.c` 中的全局变量设置目标速度：

```c
// 设置左电机目标速度（编码器计数/秒）
speed_target_A = 1000.0f;

// 设置右电机目标速度（编码器计数/秒）
speed_target_B = 1000.0f;
```

### PID参数调整

修改 `freertos.c` 中的PID控制器参数：

```c
PID_HandleTypeDef pid_A = {
    .Kp = 1.0f,   // 比例系数
    .Ki = 0.1f,   // 积分系数
    .Kd = 0.05f,  // 微分系数
    .output_min = -100.0f,
    .output_max = 100.0f
};
```

---

## 项目结构

```
ByCarAssignment/
├── .mxproject                # STM32CubeMX项目配置
├── 7.6.ioc                   # STM32CubeMX配置文件
├── README.md                 # 项目说明文档
├── Core/
│   ├── Inc/                  # 头文件目录
│   │   ├── FreeRTOSConfig.h  # FreeRTOS配置
│   │   ├── main.h            # 主程序头文件
│   │   ├── tim.h             # 定时器驱动头文件
│   │   ├── usart.h           # 串口驱动头文件
│   │   └── gpio.h            # GPIO驱动头文件
│   └── Src/                  # 源文件目录
│       ├── main.c            # 主程序入口
│       ├── freertos.c        # FreeRTOS任务和控制逻辑
│       ├── tim.c             # 定时器初始化配置
│       ├── usart.c           # 串口初始化配置
│       └── gpio.c            # GPIO初始化配置
├── Drivers/
│   ├── CMSIS/                # CMSIS核心库
│   └── STM32F1xx_HAL_Driver/ # STM32 HAL驱动库
├── MDK-ARM/                  # Keil项目文件
│   ├── 7.6.uvprojx           # Keil项目主文件
│   ├── 7.6.uvoptx            # Keil项目配置
│   └── startup_stm32f103xe.s # 启动文件
└── Middlewares/
    └── Third_Party/
        └── FreeRTOS/         # FreeRTOS实时操作系统
```

---

## FreeRTOS任务架构

| 任务名称 | 优先级 | 周期 | 功能描述 |
|----------|--------|------|----------|
| defaultTask | Normal | 1000ms | 系统初始化、状态监控输出 |
| EncoderTask | Low | 1ms | 编码器计数读取 |
| ControlMotorTas | Normal | 1ms | PWM输出控制 |
| ControlSpeedTas | Normal | 10ms | PID速度计算 |

### 数据流

```
编码器 → EncoderTask → encoder_count_A/B
    ↓
ControlSpeedTas (速度计算 → PID控制)
    ↓
motor_pwm_A/B
    ↓
ControlMotorTas → PWM输出 (TIM3_CH1~CH4)
```

---

## 常见问题解答

### Q1: 编译时出现"undefined reference to HAL_TIM_PWM_Start"

**原因**：HAL驱动库未正确添加到项目中。

**解决方案**：
1. 检查 `MDK-ARM/RTE/_7.6/RTE_Components.h` 是否包含 HAL_TIM 组件
2. 在 STM32CubeMX 中重新生成项目代码
3. 确保 `stm32f1xx_hal_tim.c` 文件已添加到编译列表

### Q2: 电机不转或转动异常

**检查步骤**：
1. 确认电机驱动板供电正常（12V）
2. 检查 PWM 输出引脚是否有信号
3. 确认编码器接线正确
4. 检查电机驱动板使能引脚是否连接

### Q3: 串口没有输出

**检查步骤**：
1. 确认串口波特率设置为 115200
2. 检查 TX/RX 引脚接线是否正确
3. 确认 USART1 初始化已执行
4. 使用示波器检查 PA9 是否有信号输出

### Q4: 速度控制不稳定

**优化建议**：
1. 调整 PID 参数（增大 Kp 提高响应，减小 Ki 防止振荡）
2. 增加编码器滤波（修改 TIM_Encoder_InitTypeDef 中的 IC1Filter）
3. 降低速度控制任务周期（当前 10ms，可尝试 5ms）
4. 检查机械传动是否存在间隙

---

## 贡献指南

欢迎贡献代码和改进建议！请遵循以下流程：

### 贡献步骤

1. **Fork 项目**：点击 GitHub 页面上的 "Fork" 按钮
2. **创建分支**：基于 `7.6` 分支创建功能分支

```bash
git checkout -b feature/your-feature-name
```

3. **提交代码**：提交你的修改，使用清晰的 commit 消息

```bash
git add .
git commit -m "feat: 添加新功能描述"
```

4. **推送分支**：推送到你的 Fork 仓库

```bash
git push origin feature/your-feature-name
```

5. **创建 Pull Request**：在 GitHub 上创建 Pull Request 到 `7.6` 分支

### 代码规范

- 遵循 STM32 HAL 库代码风格
- 使用 `/* USER CODE BEGIN */` 和 `/* USER CODE END */` 注释块
- 函数命名使用 PascalCase（如 `MX_TIM3_Init`）
- 变量命名使用 snake_case（如 `encoder_count_A`）
- 添加必要的注释说明

### PR 要求

- 代码必须通过编译测试
- 提供清晰的功能描述和测试方法
- 更新相关文档（如有必要）

---

## 许可证信息

本项目基于 STMicroelectronics HAL 库和 FreeRTOS 开源项目，遵循以下许可证：

| 组件 | 许可证 |
|------|--------|
| STM32 HAL Driver | BSD-3-Clause |
| FreeRTOS | MIT License |
| 项目源码 | MIT License |

```
MIT License

Copyright (c) 2026 ByCarAssignment

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 联系方式

- **项目地址**：https://github.com/keyuan1234/ByCarAssignment
- **提交问题**：[Issues](https://github.com/keyuan1234/ByCarAssignment/issues)
- **建议反馈**：欢迎通过 Pull Request 或 Issues 提交改进建议

---

## 更新日志

### v1.0.0 (2026-07-06)
- 初始版本发布
- 实现编码器读取功能
- 实现 PID 速度控制器
- 实现 PWM 电机驱动控制
- 添加串口调试输出功能
- 基于 FreeRTOS 实现多任务调度
