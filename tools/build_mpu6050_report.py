#!/usr/bin/env python3
"""Build a black-and-white MPU6050 experiment PDF report template."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.font_manager import FontProperties
from matplotlib.patches import Rectangle


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "output" / "pdf" / "学号_姓名.pdf"
DEFAULT_ANALYSIS_DIR = ROOT / "output" / "mpu6050"
FONT_PATHS = [
    Path("C:/Windows/Fonts/simhei.ttf"),
    Path("C:/Windows/Fonts/simsun.ttc"),
    Path("C:/Windows/Fonts/msyh.ttc"),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--analysis-dir", type=Path, default=DEFAULT_ANALYSIS_DIR)
    parser.add_argument("--student-id", default="学号")
    parser.add_argument("--student-name", default="姓名")
    return parser.parse_args()


def load_font() -> FontProperties:
    for path in FONT_PATHS:
        if path.exists():
            return FontProperties(fname=str(path))
    return FontProperties(family="sans-serif")


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def add_text(ax, x, y, text, font, size=10, weight="normal", ha="left", va="top"):
    return ax.text(
        x,
        y,
        text,
        fontproperties=font,
        fontsize=size,
        fontweight=weight,
        color="black",
        ha=ha,
        va=va,
        transform=ax.transAxes,
    )


def cjk_wrap(text: str, width: int) -> list[str]:
    lines: list[str] = []
    current = ""
    units = 0.0
    for char in text:
        char_units = 0.55 if ord(char) < 128 else 1.0
        if units + char_units > width and current:
            lines.append(current)
            current = char
            units = char_units
        else:
            current += char
            units += char_units
    if current:
        lines.append(current)
    return lines


def add_wrapped(ax, x, y, text, font, width=48, size=10, line_gap=0.035):
    current_y = y
    for line in cjk_wrap(text, width=width):
        add_text(ax, x, current_y, line, font, size=size)
        current_y -= line_gap
    return current_y


def new_page(font):
    fig, ax = plt.subplots(figsize=(8.27, 11.69))
    ax.set_axis_off()
    add_text(ax, 0.5, 0.035, "MPU6050 姿态解算实验报告", font, size=8, ha="center", va="bottom")
    return fig, ax


def draw_title_page(pdf, font, student_id, student_name):
    fig, ax = new_page(font)
    add_text(ax, 0.5, 0.88, "MPU6050 传感器通信与姿态解算实验报告", font, size=22, ha="center")
    add_text(ax, 0.5, 0.82, "黑白简洁版", font, size=12, ha="center")
    add_text(ax, 0.18, 0.68, f"学号：{student_id}", font, size=12)
    add_text(ax, 0.18, 0.63, f"姓名：{student_name}", font, size=12)
    add_text(ax, 0.18, 0.58, "平台：STM32F103 + MPU6050", font, size=12)
    add_text(ax, 0.18, 0.53, "通信：软件 I2C，PB14=SCL，PB15=SDA", font, size=12)
    add_wrapped(
        ax,
        0.18,
        0.43,
        "说明：本报告模板不包含伪造实验数据。请先烧录 MPU 实验模式固件，通过 USART1 采集 CSV，"
        "再运行 tools/mpu6050_analyze.py 生成统计表与曲线截图，最后重新生成报告。",
        font,
        width=34,
        size=10,
    )
    pdf.savefig(fig)
    plt.close(fig)


def draw_connection_page(pdf, font):
    fig, ax = new_page(font)
    add_text(ax, 0.08, 0.93, "1. MPU6050 与 STM32 的 I2C 连接", font, size=16)
    y = add_wrapped(
        ax,
        0.08,
        0.86,
        "依据 C10B 主板原理图与工程引脚定义，MPU6050 使用 PB14/PB15 作为软件 I2C 引脚，"
        "电源为 3V3，地为 GND，PB9 可作为 INT 中断输入。",
        font,
        width=44,
        size=10,
    )

    left = Rectangle((0.14, 0.48), 0.24, 0.22, fill=False, edgecolor="black", linewidth=1.2)
    right = Rectangle((0.62, 0.48), 0.24, 0.22, fill=False, edgecolor="black", linewidth=1.2)
    ax.add_patch(left)
    ax.add_patch(right)
    add_text(ax, 0.26, 0.67, "STM32F103", font, size=12, ha="center")
    add_text(ax, 0.74, 0.67, "MPU6050", font, size=12, ha="center")
    pairs = [
        ("PB14", "SCL"),
        ("PB15", "SDA"),
        ("3V3", "VCC"),
        ("GND", "GND"),
        ("PB9", "INT"),
    ]
    line_y = [0.63, 0.59, 0.55, 0.51, 0.47]
    for (src, dst), yy in zip(pairs, line_y):
        add_text(ax, 0.20, yy, src, font, size=10, ha="center", va="center")
        add_text(ax, 0.80, yy, dst, font, size=10, ha="center", va="center")
        ax.annotate(
            "",
            xy=(0.70, yy),
            xytext=(0.30, yy),
            xycoords=ax.transAxes,
            arrowprops={"arrowstyle": "-", "color": "black", "linewidth": 1},
        )
    add_text(ax, 0.08, y - 0.06, "串口输出：USART1，PA9=TX，PA10=RX，115200 8N1。", font, size=10)
    pdf.savefig(fig)
    plt.close(fig)


def draw_raw_data_page(pdf, font, analysis_dir):
    fig, ax = new_page(font)
    add_text(ax, 0.08, 0.94, "2. 原始数据读取与单位换算", font, size=16)
    add_wrapped(
        ax,
        0.08,
        0.88,
        "采集要求：静止、倾斜、快速晃动三种状态均需记录。静止状态至少取 10 组有效样本，"
        "并计算零偏与噪声标准差。",
        font,
        width=44,
        size=10,
    )

    samples = read_csv(analysis_dir / "static_samples.csv")[:10]
    columns = ["ms", "ax_raw", "ay_raw", "az_raw", "gx_raw", "gy_raw", "gz_raw", "pitch_dmp"]
    headers = ["ms", "ax", "ay", "az", "gx", "gy", "gz", "Pitch"]
    table_rows = []
    for index in range(10):
        if index < len(samples):
            row = samples[index]
            table_rows.append([row.get(column, "") for column in columns])
        else:
            table_rows.append(["待采集"] + [""] * (len(columns) - 1))
    table = ax.table(
        cellText=table_rows,
        colLabels=headers,
        loc="center",
        cellLoc="center",
        bbox=[0.07, 0.40, 0.86, 0.38],
    )
    table.auto_set_font_size(False)
    table.set_fontsize(8)
    for cell in table.get_celld().values():
        cell.set_edgecolor("black")
        cell.set_linewidth(0.6)
        cell.set_facecolor("white")
        cell.get_text().set_fontproperties(font)

    summary = read_csv(analysis_dir / "static_summary.csv")
    summary_rows = []
    wanted = ["ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps", "pitch_dmp", "pitch_comp"]
    by_column = {row.get("column"): row for row in summary}
    for column in wanted:
        row = by_column.get(column)
        if row:
            summary_rows.append([column, row.get("mean", ""), row.get("zero_bias", ""), row.get("stddev", "")])
        else:
            summary_rows.append([column, "待计算", "待计算", "待计算"])
    stats_table = ax.table(
        cellText=summary_rows,
        colLabels=["项目", "均值", "零偏", "标准差"],
        loc="center",
        cellLoc="center",
        bbox=[0.12, 0.13, 0.76, 0.22],
    )
    stats_table.auto_set_font_size(False)
    stats_table.set_fontsize(8)
    for cell in stats_table.get_celld().values():
        cell.set_edgecolor("black")
        cell.set_linewidth(0.6)
        cell.set_facecolor("white")
        cell.get_text().set_fontproperties(font)
    pdf.savefig(fig)
    plt.close(fig)


def draw_comparison_page(pdf, font, analysis_dir):
    fig, ax = new_page(font)
    add_text(ax, 0.08, 0.94, "3. DMP 与互补滤波姿态解算对比", font, size=16)
    curve = analysis_dir / "pitch_compare.png"
    if curve.exists():
        image = plt.imread(curve)
        ax.imshow(image, extent=[0.10, 0.90, 0.52, 0.84], aspect="auto", cmap="gray")
        ax.add_patch(Rectangle((0.10, 0.52), 0.80, 0.32, fill=False, edgecolor="black", linewidth=0.8))
    else:
        ax.add_patch(Rectangle((0.10, 0.52), 0.80, 0.32, fill=False, edgecolor="black", linewidth=0.8))
        add_text(ax, 0.50, 0.68, "待插入上位机姿态曲线截图", font, size=12, ha="center", va="center")

    rows = [
        ["噪声大小", "静止段更平滑，标准差较小。", "会受加速度瞬时噪声影响。"],
        ["响应延迟", "输出稳定，快速变化时略滞后。", "陀螺积分项响应较快。"],
        ["长期漂移", "内部融合与校准可抑制漂移。", "受陀螺零偏影响，长时可能漂移。"],
    ]
    table = ax.table(
        cellText=rows,
        colLabels=["维度", "DMP", "互补滤波"],
        loc="center",
        cellLoc="left",
        bbox=[0.07, 0.19, 0.86, 0.24],
    )
    table.auto_set_font_size(False)
    table.set_fontsize(8)
    for cell in table.get_celld().values():
        cell.set_edgecolor("black")
        cell.set_linewidth(0.6)
        cell.set_facecolor("white")
        cell.get_text().set_fontproperties(font)
    add_wrapped(
        ax,
        0.08,
        0.12,
        "正负方向验证：按实验要求，前倾 Pitch 应为负，后倾 Pitch 应为正。若实物安装方向相反，"
        "在固件中通过 MPU_PITCH_OUTPUT_SIGN 统一修正输出方向。",
        font,
        width=46,
        size=9,
    )
    pdf.savefig(fig)
    plt.close(fig)


def draw_fusion_page(pdf, font):
    fig, ax = new_page(font)
    add_text(ax, 0.08, 0.94, "4. 数据融合核心思想", font, size=16)
    paragraphs = [
        "加速度计能通过重力方向估计倾角，低频稳定、长期不漂移，但在快速晃动或受到线加速度干扰时噪声明显。",
        "陀螺仪测量角速度，积分后可得到角度，短时响应快、动态性能好，但零偏会随时间积分成漂移。",
        "互补滤波把两者按频率特性融合：低频信任加速度计，高频信任陀螺仪。DMP 则在芯片内部完成更复杂的六轴融合与校准，主控只需读取四元数并换算欧拉角。",
    ]
    y = 0.86
    for paragraph in paragraphs:
        y = add_wrapped(ax, 0.10, y, paragraph, font, width=38, size=11, line_gap=0.04)
        y -= 0.035
    add_text(ax, 0.08, 0.38, "提交前检查", font, size=13)
    checklist = [
        "已采集 static、tilt、shake 三段 CSV 数据。",
        "静止状态有效样本不少于 10 组。",
        "已运行 tools/mpu6050_analyze.py 并生成统计表和曲线图。",
        "已把占位学号、姓名替换为真实信息。",
        "PDF 全文和表格为黑白，无伪造数据。",
    ]
    y = 0.33
    for item in checklist:
        add_text(ax, 0.12, y, f"[ ] {item}", font, size=10)
        y -= 0.045
    pdf.savefig(fig)
    plt.close(fig)


def main() -> int:
    args = parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    font = load_font()
    with PdfPages(args.out) as pdf:
        draw_title_page(pdf, font, args.student_id, args.student_name)
        draw_connection_page(pdf, font)
        draw_raw_data_page(pdf, font, args.analysis_dir)
        draw_comparison_page(pdf, font, args.analysis_dir)
        draw_fusion_page(pdf, font)
    print(args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
