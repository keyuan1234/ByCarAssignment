#!/usr/bin/env python3
"""Analyze MPU6050 experiment CSV exported by the STM32 firmware."""

from __future__ import annotations

import argparse
import csv
import math
import statistics
from pathlib import Path


NUMERIC_COLUMNS = [
    "ax_raw",
    "ay_raw",
    "az_raw",
    "gx_raw",
    "gy_raw",
    "gz_raw",
    "ax_g",
    "ay_g",
    "az_g",
    "ax_ms2",
    "ay_ms2",
    "az_ms2",
    "gx_dps",
    "gy_dps",
    "gz_dps",
    "pitch_acc",
    "pitch_comp",
    "pitch_dmp",
    "roll_dmp",
    "yaw_dmp",
]

CSV_COLUMNS = [
    "ms",
    "state",
    "ax_raw",
    "ay_raw",
    "az_raw",
    "gx_raw",
    "gy_raw",
    "gz_raw",
    "ax_g",
    "ay_g",
    "az_g",
    "ax_ms2",
    "ay_ms2",
    "az_ms2",
    "gx_dps",
    "gy_dps",
    "gz_dps",
    "pitch_acc",
    "pitch_comp",
    "pitch_dmp",
    "roll_dmp",
    "yaw_dmp",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compute static noise/bias statistics and plot pitch curves."
    )
    parser.add_argument("csv_file", type=Path, help="CSV captured from USART1")
    parser.add_argument("--out-dir", type=Path, default=Path("output/mpu6050"))
    parser.add_argument("--static-state", default="static")
    parser.add_argument("--static-count", type=int, default=10)
    parser.add_argument(
        "--state-override",
        help="Override the state column for captures saved without changing firmware labels.",
    )
    return parser.parse_args()


def read_rows(path: Path, state_override: str | None = None) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        has_header = False
        filtered_lines = []
        for line in handle:
            if not line.strip() or line.startswith("#"):
                continue
            if line.startswith("ms,state,"):
                has_header = True
                filtered_lines.append(line)
                continue
            if has_header:
                filtered_lines.append(line)
            else:
                parts = next(csv.reader([line]))
                if len(parts) == len(CSV_COLUMNS):
                    try:
                        float(parts[0])
                    except ValueError:
                        continue
                    filtered_lines.append(line)
        filtered = iter(filtered_lines)
        reader = csv.DictReader(filtered) if has_header else csv.DictReader(filtered, fieldnames=CSV_COLUMNS)
        for row in reader:
            if not row:
                continue
            if state_override:
                row["state"] = state_override
            for key in NUMERIC_COLUMNS + ["ms"]:
                if key in row and row[key] not in ("", None):
                    try:
                        row[key] = str(float(row[key]))
                    except ValueError:
                        row[key] = ""
            rows.append(row)
    return rows


def write_static_samples(rows: list[dict[str, str]], out_path: Path) -> None:
    columns = [
        "ms",
        "state",
        "ax_raw",
        "ay_raw",
        "az_raw",
        "gx_raw",
        "gy_raw",
        "gz_raw",
        "ax_g",
        "ay_g",
        "az_g",
        "gx_dps",
        "gy_dps",
        "gz_dps",
        "pitch_dmp",
        "pitch_comp",
    ]
    with out_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        for row in rows:
            writer.writerow({column: row.get(column, "") for column in columns})


def summarize(rows: list[dict[str, str]], out_path: Path) -> None:
    with out_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["column", "mean", "zero_bias", "stddev"])
        for column in NUMERIC_COLUMNS:
            values = [float(row[column]) for row in rows if row.get(column)]
            if len(values) < 2:
                continue
            mean = statistics.fmean(values)
            stddev = statistics.stdev(values)
            zero_bias = mean
            if column == "az_g":
                zero_bias = mean - (1.0 if mean >= 0.0 else -1.0)
            elif column == "az_ms2":
                zero_bias = mean - (9.80665 if mean >= 0.0 else -9.80665)
            writer.writerow([column, f"{mean:.6f}", f"{zero_bias:.6f}", f"{stddev:.6f}"])


def plot_pitch(rows: list[dict[str, str]], out_path: Path) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise RuntimeError("matplotlib is required for curve PNG output") from exc

    times = [float(row["ms"]) / 1000.0 for row in rows if row.get("ms")]
    pitch_dmp = [float(row["pitch_dmp"]) for row in rows if row.get("pitch_dmp")]
    pitch_comp = [float(row["pitch_comp"]) for row in rows if row.get("pitch_comp")]
    count = min(len(times), len(pitch_dmp), len(pitch_comp))
    if count == 0:
        raise RuntimeError("no pitch samples found")

    plt.figure(figsize=(8, 4.2), dpi=160)
    plt.plot(times[:count], pitch_dmp[:count], color="black", linewidth=1.4, label="DMP Pitch")
    plt.plot(
        times[:count],
        pitch_comp[:count],
        color="black",
        linewidth=1.0,
        linestyle="--",
        label="Complementary Pitch",
    )
    plt.xlabel("Time (s)")
    plt.ylabel("Pitch (deg)")
    plt.grid(True, color="0.80", linestyle="-", linewidth=0.5)
    plt.legend(frameon=False)
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def main() -> int:
    args = parse_args()
    rows = read_rows(args.csv_file, args.state_override)
    if not rows:
        raise SystemExit("No CSV rows found. Capture USART1 output first.")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    static_rows = [row for row in rows if row.get("state") == args.static_state]
    if len(static_rows) < args.static_count:
        raise SystemExit(
            f"Need at least {args.static_count} '{args.static_state}' rows, "
            f"found {len(static_rows)}."
        )

    static_rows = static_rows[: args.static_count]
    write_static_samples(static_rows, args.out_dir / "static_samples.csv")
    summarize(static_rows, args.out_dir / "static_summary.csv")
    try:
        plot_pitch(rows, args.out_dir / "pitch_compare.png")
    except RuntimeError as exc:
        print(f"Warning: {exc}")
    print(f"Wrote analysis files to {args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
