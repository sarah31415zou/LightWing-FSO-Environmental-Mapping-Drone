"""Offline summaries for telemetry CSV files produced by the host logger."""

from __future__ import annotations

import csv
import math
import statistics
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class LinkSummary:
    """A compact, serializable summary of one ground-station recording."""

    samples: int
    received_frames: int
    crc_passes: int
    crc_failures: int
    crc_pass_rate: float | None
    elapsed_seconds: float | None
    mean_ber_ppm: float | None
    median_ber_ppm: float | None
    mean_rssi: float | None
    min_rssi: float | None
    max_rssi: float | None

    def to_dict(self) -> dict[str, object]:
        """Return JSON-ready values without dataclass implementation details."""

        return asdict(self)


def _number(value: str | None) -> float | None:
    if value is None:
        return None
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    return result if math.isfinite(result) else None


def _values(rows: Iterable[dict[str, str]], column: str) -> list[float]:
    values: list[float] = []
    for row in rows:
        value = _number(row.get(column))
        if value is not None:
            values.append(value)
    return values


def summarize_rows(rows: Iterable[dict[str, str]]) -> LinkSummary:
    """Summarize records in the CsvLogger schema.

    RX rows are treated as received frames. The firmware represents CRC status
    as 1 for pass and 0 for failure; missing or malformed values remain outside
    the CRC-rate denominator.
    """

    records = list(rows)
    received = [row for row in records if row.get("event", "").upper() == "RX"]
    crc_values = [_number(row.get("crc")) for row in received]
    known_crc = [value for value in crc_values if value is not None]
    crc_passes = sum(value == 1 for value in known_crc)
    crc_failures = sum(value == 0 for value in known_crc)
    crc_rate = (crc_passes / len(known_crc)) if known_crc else None

    timestamps = _values(records, "ts")
    elapsed = (max(timestamps) - min(timestamps)) if len(timestamps) > 1 else None
    ber = _values(received, "ber_ppm")
    rssi = _values(received, "rssi")

    return LinkSummary(
        samples=len(records),
        received_frames=len(received),
        crc_passes=crc_passes,
        crc_failures=crc_failures,
        crc_pass_rate=crc_rate,
        elapsed_seconds=elapsed,
        mean_ber_ppm=statistics.fmean(ber) if ber else None,
        median_ber_ppm=statistics.median(ber) if ber else None,
        mean_rssi=statistics.fmean(rssi) if rssi else None,
        min_rssi=min(rssi) if rssi else None,
        max_rssi=max(rssi) if rssi else None,
    )


def summarize_csv(path: Path) -> LinkSummary:
    """Read a CsvLogger recording and return its link-quality summary."""

    with path.open(newline="", encoding="utf-8") as handle:
        return summarize_rows(csv.DictReader(handle))
