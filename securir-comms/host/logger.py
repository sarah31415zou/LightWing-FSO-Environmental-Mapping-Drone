from __future__ import annotations

import csv
import time
from pathlib import Path
from typing import Iterable


class CsvLogger:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._file = self.path.open("a", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)
        if self.path.stat().st_size == 0:
            self._writer.writerow(["ts", "event", "seq", "len", "crc", "ber_ppm", "rssi", "info"])
            self._file.flush()

    def log(self, event: str, seq: str = "", length: str = "", crc: str = "", ber_ppm: str = "", rssi: str = "", info: str = "") -> None:
        self._writer.writerow([f"{time.time():.3f}", event, seq, length, crc, ber_ppm, rssi, info])
        self._file.flush()

    def close(self) -> None:
        self._file.close()


def parse_eye_values(value: str) -> list[int]:
    if not value:
        return []
    out = []
    for token in value.split(","):
        token = token.strip()
        if not token:
            continue
        try:
            out.append(int(token))
        except ValueError:
            continue
    return out


def moving_average(values: Iterable[float], window: int) -> list[float]:
    vals = list(values)
    if window <= 1:
        return vals
    out: list[float] = []
    acc = 0.0
    q: list[float] = []
    for v in vals:
        q.append(v)
        acc += v
        if len(q) > window:
            acc -= q.pop(0)
        out.append(acc / len(q))
    return out
