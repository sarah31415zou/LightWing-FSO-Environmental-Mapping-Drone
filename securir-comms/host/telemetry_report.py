"""Command-line report generator for SecurIR telemetry recordings."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from telemetry_analysis import LinkSummary, summarize_csv


def _format_number(value: float | None, digits: int = 2) -> str:
    return "n/a" if value is None else f"{value:.{digits}f}"


def format_summary(summary: LinkSummary) -> str:
    """Render a compact human-readable report."""

    crc_rate = (
        "n/a"
        if summary.crc_pass_rate is None
        else f"{summary.crc_pass_rate * 100:.2f}%"
    )
    return "\n".join(
        [
            "SecurIR telemetry summary",
            f"Samples:          {summary.samples}",
            f"Received frames:  {summary.received_frames}",
            f"CRC passes:       {summary.crc_passes}",
            f"CRC failures:     {summary.crc_failures}",
            f"CRC pass rate:    {crc_rate}",
            f"Elapsed:          {_format_number(summary.elapsed_seconds)} s",
            f"Mean BER:         {_format_number(summary.mean_ber_ppm)} ppm",
            f"Median BER:       {_format_number(summary.median_ber_ppm)} ppm",
            f"Mean RSSI proxy:  {_format_number(summary.mean_rssi)}",
            f"RSSI proxy range: {_format_number(summary.min_rssi)} to {_format_number(summary.max_rssi)}",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Summarize a CSV recording made by the SecurIR host GUI."
    )
    parser.add_argument("csv", type=Path, help="path to a telemetry CSV file")
    parser.add_argument(
        "--json",
        action="store_true",
        help="print machine-readable JSON instead of a text report",
    )
    args = parser.parse_args()

    summary = summarize_csv(args.csv)
    if args.json:
        print(json.dumps(summary.to_dict(), indent=2, sort_keys=True))
    else:
        print(format_summary(summary))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
