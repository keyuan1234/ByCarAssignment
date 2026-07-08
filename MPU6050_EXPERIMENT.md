# MPU6050 姿态解算实验使用说明

## 1. 固件模式切换

默认固件仍运行原有电机实验。要启用 MPU6050 实验，在 Keil/CubeIDE 的 C 预处理宏中加入：

```text
APP_EXPERIMENT_MODE=APP_EXPERIMENT_MPU
```

如需标记当前采集状态，可重新编译时加入其中一个状态宏：

```text
MPU_SAMPLE_STATE=\"static\"
MPU_SAMPLE_STATE=\"tilt\"
MPU_SAMPLE_STATE=\"shake\"
```

若实测 Pitch 方向与实验要求相反，加入：

```text
MPU_PITCH_OUTPUT_SIGN=-1
```

默认方向为 `前倾为负、后倾为正`。

## 2. 串口采集

串口参数：

```text
USART1, 115200, 8N1
PA9=TX, PA10=RX
```

MPU6050 连接：

```text
PB14 -> SCL
PB15 -> SDA
3V3  -> VCC
GND  -> GND
PB9  -> INT
```

固件输出 CSV 表头：

```text
ms,state,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw,ax_g,ay_g,az_g,ax_ms2,ay_ms2,az_ms2,gx_dps,gy_dps,gz_dps,pitch_acc,pitch_comp,pitch_dmp,roll_dmp,yaw_dmp
```

分别采集三段数据：静止、倾斜、快速晃动。静止段至少保留 10 组有效样本。

## 3. 数据分析

保存串口日志为 CSV 后运行：

```powershell
python tools\mpu6050_analyze.py path\to\capture.csv --out-dir output\mpu6050
```

脚本会生成：

```text
output/mpu6050/static_samples.csv
output/mpu6050/static_summary.csv
output/mpu6050/pitch_compare.png
```

## 4. 生成报告

使用占位学号和姓名：

```powershell
python tools\build_mpu6050_report.py
```

替换为真实学号和姓名：

```powershell
python tools\build_mpu6050_report.py --student-id 你的学号 --student-name 你的姓名
```

输出文件：

```text
output/pdf/学号_姓名.pdf
```
