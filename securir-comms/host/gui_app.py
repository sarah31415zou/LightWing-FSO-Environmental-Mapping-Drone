from __future__ import annotations

import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

from logger import CsvLogger, parse_eye_values
from protocol import parse_rx_line
from serial_link import SerialConfig, SerialLineLink


class App:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("ESP32 IR FSO MVP")
        self.root.geometry("1100x760")

        self.tx_link: SerialLineLink | None = None
        self.rx_link: SerialLineLink | None = None
        self.worker_stop = threading.Event()

        self.rssi_hist: list[float] = []
        self.ber_hist: list[float] = []
        self.eye_buf: list[int] = []

        ts = int(time.time())
        self.logger = CsvLogger(Path(f"logs/session_{ts}.csv"))

        self._build_ui()
        self._redraw_loop()

    def _build_ui(self) -> None:
        top = ttk.Frame(self.root)
        top.pack(fill=tk.X, padx=8, pady=8)

        self.tx_port_var = tk.StringVar(value="COM3")
        self.rx_port_var = tk.StringVar(value="COM4")
        self.bitrate_var = tk.IntVar(value=2000)
        self.payload_var = tk.StringVar(value="HELLO_FSO")

        ttk.Label(top, text="TX Port").grid(row=0, column=0, sticky="w")
        ttk.Entry(top, textvariable=self.tx_port_var, width=12).grid(row=0, column=1, padx=4)
        ttk.Label(top, text="RX Port").grid(row=0, column=2, sticky="w")
        ttk.Entry(top, textvariable=self.rx_port_var, width=12).grid(row=0, column=3, padx=4)

        ttk.Label(top, text="Bitrate").grid(row=0, column=4, sticky="w")
        ttk.Combobox(top, textvariable=self.bitrate_var, values=[500, 1000, 2000, 5000], width=10).grid(row=0, column=5, padx=4)

        ttk.Button(top, text="Connect", command=self.connect).grid(row=0, column=6, padx=6)
        ttk.Button(top, text="Disconnect", command=self.disconnect).grid(row=0, column=7, padx=6)

        ttk.Entry(top, textvariable=self.payload_var, width=32).grid(row=1, column=0, columnspan=4, sticky="we", padx=4, pady=6)
        ttk.Button(top, text="Send Once", command=self.send_once).grid(row=1, column=4, padx=4)
        ttk.Button(top, text="Start Pattern", command=self.start_pattern).grid(row=1, column=5, padx=4)
        ttk.Button(top, text="Stop", command=self.stop_pattern).grid(row=1, column=6, padx=4)

        self.log_text = tk.Text(self.root, height=12)
        self.log_text.pack(fill=tk.X, padx=8, pady=8)

        fig, (self.ax_rssi, self.ax_ber, self.ax_eye) = plt.subplots(3, 1, figsize=(10, 6))
        fig.tight_layout(pad=2.0)
        self.canvas = FigureCanvasTkAgg(fig, master=self.root)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True, padx=8, pady=8)

    def log(self, text: str) -> None:
        self.log_text.insert(tk.END, text + "\n")
        self.log_text.see(tk.END)

    def connect(self) -> None:
        try:
            bitrate = int(self.bitrate_var.get())
            self.tx_link = SerialLineLink(SerialConfig(port=self.tx_port_var.get().strip()))
            self.rx_link = SerialLineLink(SerialConfig(port=self.rx_port_var.get().strip()))
            self.tx_link.open()
            self.rx_link.open()
            self.tx_link.send_line(f"BITRATE {bitrate}")
            self.rx_link.send_line(f"BITRATE {bitrate}")
            self.worker_stop.clear()
            threading.Thread(target=self._poll_worker, daemon=True).start()
            self.log("Connected")
        except Exception as exc:
            messagebox.showerror("Connect failed", str(exc))

    def disconnect(self) -> None:
        self.worker_stop.set()
        if self.tx_link:
            self.tx_link.close()
            self.tx_link = None
        if self.rx_link:
            self.rx_link.close()
            self.rx_link = None
        self.log("Disconnected")

    def send_once(self) -> None:
        if not self.tx_link:
            messagebox.showwarning("Not connected", "Connect first")
            return
        payload = self.payload_var.get().encode("utf-8", errors="ignore")
        self.tx_link.send_line("SEND " + payload.hex())
        self.log(f"TX SEND {payload!r}")

    def start_pattern(self) -> None:
        if not self.tx_link:
            return
        self.tx_link.send_line("PATTERN START")
        self.log("TX pattern started")

    def stop_pattern(self) -> None:
        if not self.tx_link:
            return
        self.tx_link.send_line("PATTERN STOP")
        self.log("TX pattern stopped")

    def _poll_worker(self) -> None:
        while not self.worker_stop.is_set():
            if self.rx_link:
                line = self.rx_link.get_line(timeout=0.02)
                if line:
                    self._handle_rx_line(line)
            if self.tx_link:
                line = self.tx_link.get_line(timeout=0.0)
                if line:
                    self.root.after(0, self.log, "TX> " + line)
            time.sleep(0.005)

    def _handle_rx_line(self, line: str) -> None:
        parsed = parse_rx_line(line)
        if parsed.get("type") == "RX":
            seq = parsed.get("seq", "")
            length = parsed.get("len", "")
            crc = parsed.get("crc", "")
            ber_ppm = parsed.get("ber_ppm", "0")
            rssi = parsed.get("rssi", "0")
            eye = parsed.get("eye", "")

            try:
                self.ber_hist.append(float(ber_ppm) / 1_000_000.0)
                self.rssi_hist.append(float(rssi))
            except ValueError:
                pass

            e = parse_eye_values(eye)
            if e:
                self.eye_buf = e

            self.logger.log("RX", seq, length, crc, ber_ppm, rssi, parsed.get("hex", ""))
            self.root.after(0, self.log, "RX> " + line)
        else:
            self.root.after(0, self.log, "RX> " + line)

    def _redraw_loop(self) -> None:
        self.ax_rssi.clear()
        self.ax_rssi.set_title("RSSI Proxy")
        if self.rssi_hist:
            self.ax_rssi.plot(self.rssi_hist[-200:])

        self.ax_ber.clear()
        self.ax_ber.set_title("BER Estimate")
        if self.ber_hist:
            self.ax_ber.semilogy(np.clip(self.ber_hist[-200:], 1e-7, 1.0))
            self.ax_ber.set_ylim(1e-7, 1)

        self.ax_eye.clear()
        self.ax_eye.set_title("Eye-like Symbol Overlay")
        if self.eye_buf:
            arr = np.array(self.eye_buf, dtype=float)
            span = 16
            chunks = len(arr) // span
            for i in range(min(chunks, 40)):
                s = arr[i * span : (i + 1) * span]
                self.ax_eye.plot(s, alpha=0.3)

        self.canvas.draw_idle()
        self.root.after(400, self._redraw_loop)

    def on_close(self) -> None:
        self.disconnect()
        self.logger.close()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    app = App(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


if __name__ == "__main__":
    main()
