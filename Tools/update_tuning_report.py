from pathlib import Path

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
from docx.shared import Pt


def set_cell(cell, text, center=False):
    cell.text = text
    for paragraph in cell.paragraphs:
        paragraph.alignment = (
            WD_ALIGN_PARAGRAPH.CENTER if center else WD_ALIGN_PARAGRAPH.LEFT
        )
        paragraph.paragraph_format.space_after = Pt(0)
        for run in paragraph.runs:
            run.font.name = "Microsoft YaHei"
            run._element.get_or_add_rPr().rFonts.set(
                qn("w:eastAsia"), "Microsoft YaHei"
            )
            run.font.size = Pt(9)


def set_table_rows(table, rows):
    if len(table.rows) != len(rows):
        raise RuntimeError(
            f"Unexpected table row count: {len(table.rows)} != {len(rows)}"
        )
    for row_index, values in enumerate(rows):
        if len(table.rows[row_index].cells) != len(values):
            raise RuntimeError("Unexpected table column count")
        for col_index, value in enumerate(values):
            set_cell(
                table.rows[row_index].cells[col_index],
                value,
                center=(row_index == 0 or col_index == 0),
            )


root = Path.cwd() / "ByCarStable"
source = root / "平衡小车级联PID调参报告.docx"
output = root / "平衡小车级联PID调参报告_实测版.docx"
doc = Document(source)

replacements = {
    3: (
        "完成两轮平衡小车的级联 PID 控制参数整定流程。先屏蔽速度环和转向环，"
        "仅保留角度环 PD，以稳定站立不少于 5 s 为阶段目标；再开启速度环 PI，"
        "使小车从单纯站住过渡到基本静止，并具备轻推后回正的效果。"
    ),
    10: (
        "Balance_Out = Balance_Kp × (pitch_deg - middle_angle) "
        "+ Balance_Kd × gyro_pitch_dps"
    ),
    11: (
        "pitch_deg 由 MPU6050 原始六轴数据以 5 ms 周期进行互补滤波得到，"
        "gyro_pitch_dps 由 X 轴陀螺仪按 16.4 LSB/(deg/s) 换算。WHEELTEC 示例的 "
        "Balance_Kd 作用于原始陀螺计数，本工程则作用于 deg/s，二者不能直接按同一数值照搬。"
    ),
    13: (
        "速度环作为外环，输入为左右编码器 10 ms 增量之和。编码器速度经 "
        "0.84/0.16 低通滤波后，速度 PI 形成目标角度修正；角度 PD 内环比较目标角和当前角度，"
        "并作为唯一的电机 PWM 输出环节。"
    ),
    14: (
        "Velocity_Angle = limit(Velocity_Kp × encoder_bias + Velocity_Ki × encoder_integral, -1.5°, +1.5°)；"
        "Target_Angle = Middle_Angle + Velocity_Angle；Balance_Out = PD(pitch - Target_Angle, gyro)。"
    ),
    15: (
        "车体相对机械中值的角度误差超过 2° 后线性削弱速度外环，到 4° 时外环输出与积分清零，"
        "目标角回到机械中值，保证角度内环优先扶正。积分采用条件累加抗饱和，目标角修正每 10 ms "
        "最多变化 0.05°。转向环保持屏蔽，Turn_Out 固定为 0。"
    ),
    17: (
        "调试阶段选择 ANGLE_PD_ONLY，速度环与转向环输出均为 0。固件预置 1–4 号角度环档位，"
        "每次只改变 balance_control.h 中的 BALANCE_TUNING_PROFILE，串口 #config 行自动记录"
        "本次模式和参数。判定稳定需同时满足：连续受控不少于 5 s、角度误差不超过 5°、PWM 不饱和。"
    ),
    19: (
        "只有角度环试验达到 5 s 验收后才进入速度环。先把通过验收的 Kp/Kd 写入 "
        "BALANCE_SELECTED_ANGLE_KP/KD，5、6 号档始终引用这组参数，依次尝试 "
        "Velocity_Kp=0.003、Velocity_Ki=0，"
        "以及 Velocity_Kp=0.006、Velocity_Ki=0.00002。开启后先架空确认正编码器速度产生正 "
        "velocity_angle，并使直立附近的 balance_pwm 产生反向制动，再落地测试。"
    ),
    20: "六、当前候选参数与整定顺序",
    22: (
        "固件通过 USART1 输出 CSV：ms、pitch、gyro、enc_l、enc_r、target_angle、"
        "velocity_angle、balance_pwm、pwm_l、pwm_r、state。启动时另输出 #config，其中包含 "
        "trial、mode、middle、balance_kp、balance_kd、velocity_kp、velocity_ki 和角度修正限幅。"
    ),
    23: (
        "验收时先运行 ANGLE_PD_ONLY，确认小车连续受控站立不少于 5 s，且振幅不随时间增长；"
        "再切换 CASCADE_PI_PD，确认小车基本静止、被轻推后能回正。Tools/Analyze-BalanceLog.ps1 "
        "统一计算受控时长、角度 RMS、前后段 RMS 和 PWM 饱和次数。"
    ),
    25: (
        "第二次串级实测使用角度 Kp=300、Kd=8，速度 Kp=0.006、Ki=0：按车身偏差 ±5° 的新标准，"
        "有效受控仅约 0.80 s，角度 RMS 为 12.39°，最大误差 39.11°，判定为增长振荡。当前固件"
        "改为角度 Kp=300、Kd=12，速度 Kp=0.003、Ki=0，目标角限幅 ±1.5°、每 10 ms 最大变化 "
        "0.05°；车身误差从 2° 起线性削弱外环，到 4° 完全退出。"
    ),
    26: (
        "由于稳定性强依赖电池电压、机械重心、轮胎摩擦和 MPU6050 安装方向，"
        "第 3–6 次记录必须由实车串口日志补齐，不把预期现象写成已验证结论。"
        "若速度环出现低频摆动，应先将 Ki 归零，再区分比例过强与积分累积问题。"
    ),
}

for index, text in replacements.items():
    doc.paragraphs[index].text = text

set_table_rows(
    doc.tables[0],
    [
        ["项目", "内容"],
        ["工程", "D:/实践任务/ByCAR/ByCarStable"],
        ["日期", "2026 年 07 月 10 日"],
        ["控制结构", "速度 PI -> 目标角修正 -> 角度 PD -> 左右电机 PWM；转向环关闭"],
        ["记录性质", "第 1、2 次为已有实车反馈/日志；第 3–6 次为待烧录实测档位。"],
    ],
)

set_table_rows(
    doc.tables[1],
    [[
        "说明：报告不会把推测现象当成实测结果。第 3–6 次只有在对应 #config 与 CSV 日志"
        "保存后，才能填写最终现象并判定是否达到“站立 ≥5 s”和“站得住、推得回”。"
    ]],
)

hardware_rows = [
    ["功能", "主控资源", "说明"],
    ["左电机 PWM", "PA6/TIM3_CH1, PA7/TIM3_CH2", "两路反相 PWM 控制左轮方向和等效电压"],
    ["右电机 PWM", "PB0/TIM3_CH3, PB1/TIM3_CH4", "两路反相 PWM 控制右轮方向和等效电压"],
    ["左轮编码器", "PB6/PB7, TIM4 Encoder", "10 ms 正交编码器增量"],
    ["右轮编码器", "PC6/PC7, TIM8 Encoder", "方向宏修正后与左轮同向"],
    ["姿态传感器", "MPU6050, PB14/PB15 软件 I2C", "原始六轴，5 ms 互补滤波输出 pitch 与 gyro"],
    ["串口日志", "USART1, 115200 bps", "输出 #config、控制 CSV 与安全状态"],
]
set_table_rows(doc.tables[2], hardware_rows)

pd_rows = [
    ["次数", "参数", "观察到的现象", "分析与下一步决策"],
    [
        "1",
        "ANGLE_PD_ONLY\nKp=270, Kd=1.10",
        "已实测：开始可短时站立，随后振幅逐渐增加。",
        "Kd 直接沿用原始陀螺计数示例后，在 deg/s 单位下阻尼明显不足；增大 Kd。",
    ],
    [
        "2",
        "ANGLE_PD_ONLY\nKp=370, Kd=6.56",
        "已实测：约 3.3 s 后振幅增大；4.28 s 角误差超过 10°，4.48 s PWM 饱和，5.43 s TILT。",
        "比例偏硬且阻尼不足；有效受控仅约 4.00 s。降低 Kp、提高 Kd。",
    ],
    [
        "3",
        "ANGLE_PD_ONLY\nKp=300, Kd=12.0",
        "待实测：记录软塌塌/振荡/稳定/僵硬及受控时长。",
        "角度环候选档。若仍增长振荡进入第 4 次；若不振但偏软，仅小步增加 Kp。",
    ],
    [
        "4",
        "ANGLE_PD_ONLY\nKp=300, Kd=18.0",
        "待实测：目标为振幅不增长且连续受控 ≥5 s。",
        "作为更强阻尼档。通过后冻结角度参数，开始速度环；若高频抖动则回退 Kd。",
    ],
    [
        "手动复核",
        "PROFILE=0\nKp 每次 ±10~20；Kd 每次 ±2~3",
        "仅在第 3、4 次无法明确归类时使用。",
        "一次只改一个参数，保留 #config 和完整 CSV，禁止同时修改机械中值。",
    ],
]
set_table_rows(doc.tables[3], pd_rows)

velocity_rows = [
    ["次数", "参数", "观察到的现象", "分析与下一步决策"],
    [
        "5",
        "CASCADE_PI_PD\nB=300/5; V_Kp=0.010, V_Ki=0.020",
        "已实测：有效受控 2.20 s，角度 RMS 11.12°；目标角限幅占 67.7%，PWM 饱和 19 条，最终 TILT。",
        "Ki 对角度输出结构过大，外环成为满幅切换；Ki 归零、降低 Kp，并加入抗饱和与斜率限制。",
    ],
    [
        "6",
        "CASCADE_PI_PD\nB=300/8; V_Kp=0.006, V_Ki=0; limit=±4°",
        "已实测：按 ±5° 标准仅受控 0.80 s；角度 RMS 12.39°、最大误差 39.11°，振幅持续增长。",
        "外环允许角度仍过大且内环阻尼偏低；缩小目标角、提前渐退外环并提高 Kd。",
    ],
    [
        "7（可选）",
        "PROFILE=5\nB=300/12; V_Kp=0.003, V_Ki=0; limit=±1.5°",
        "待复测：目标为车身偏差保持 ±5° 内至少 5 s，且振幅不增长。",
        "当前固件档。通过前不增加积分，也不放宽目标角限幅。",
    ],
    [
        "8（可选）",
        "PROFILE=6\nV_Kp=0.006, V_Ki=0.00002",
        "仅在第 7 次通过后测试“站得住、推得回”。",
        "若出现低频摆动立即将 Ki 归零；不得放宽 ±1.5° 目标角限制。",
    ],
]
set_table_rows(doc.tables[4], velocity_rows)

parameter_rows = [
    ["参数", "当前候选/限制", "作用", "备注"],
    ["Balance_Kp", "300.0", "角度误差比例增益", "第 3、4 次保持不变"],
    ["Balance_Kd", "12.0（当前）", "角速度阻尼增益", "作用于 deg/s，非原始计数"],
    ["Velocity_Kp", "0.003 -> 0.006", "速度偏差到目标角的比例增益", "输出单位为角度，不是 PWM"],
    ["Velocity_Ki", "0 -> 0.00002", "消除长期速度偏差", "出现低频摆动时先归零"],
    ["Mechanical_Mid_Angle", "-1.0°，每次微调 0.1~0.2°", "补偿机械重心偏置", "PID 稳定后再调整"],
    ["Velocity angle limit", "±1.5°；每 10 ms 变化 ≤0.05°", "限制外环目标角修正", "误差 2° 起渐退，4° 时外环清零"],
    ["Fall protect", "±40°", "跌倒保护阈值", "超限立即停电机"],
]
set_table_rows(doc.tables[5], parameter_rows)

doc.core_properties.title = "平衡小车级联 PID 控制参数整定报告（实测版）"
doc.core_properties.subject = "ByCarStable angle PD and velocity PI tuning"
doc.save(output)
print(output)
