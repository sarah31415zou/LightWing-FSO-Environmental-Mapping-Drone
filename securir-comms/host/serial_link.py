from __future__ import annotations

import queue
import threading
import time
from dataclasses import dataclass
from typing import Optional

import serial


@dataclass
class SerialConfig:
    port: str
    baud: int = 115200
    timeout: float = 0.05


class SerialLineLink:
    def __init__(self, cfg: SerialConfig) -> None:
        self.cfg = cfg
        self.ser: Optional[serial.Serial] = None
        self._rx_queue: "queue.Queue[str]" = queue.Queue()
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def open(self) -> None:
        self.ser = serial.Serial(self.cfg.port, self.cfg.baud, timeout=self.cfg.timeout)
        self._stop.clear()
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._stop.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=0.5)
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.ser = None

    def send_line(self, line: str) -> None:
        if not self.ser or not self.ser.is_open:
            raise RuntimeError("serial not open")
        self.ser.write((line.strip() + "\n").encode("utf-8", errors="ignore"))
        self.ser.flush()

    def get_line(self, timeout: float = 0.0) -> Optional[str]:
        try:
            return self._rx_queue.get(timeout=timeout)
        except queue.Empty:
            return None

    def _reader(self) -> None:
        assert self.ser is not None
        while not self._stop.is_set():
            try:
                raw = self.ser.readline()
                if raw:
                    line = raw.decode("utf-8", errors="replace").strip()
                    if line:
                        self._rx_queue.put(line)
                else:
                    time.sleep(0.005)
            except Exception:
                time.sleep(0.05)
