# Code Structure Guide

This document explains how the repository is organized and how data flows across firmware and host tools.

## 1) Top-Level Layout

- `firmware/common/`
  - Shared frame and CRC logic used by TX/RX firmware.
  - Optional Hamming(7,4) helper implementation.
- `firmware/arduino/`
  - Arduino IDE firmware for ESP32-WROOM dev boards.
  - `tx/tx.ino` transmitter sketch.
  - `rx/rx.ino` receiver sketch.
- `firmware/tx/`
  - ESP-IDF transmitter app (legacy path).
  - Reads serial commands and transmits Manchester-coded OOK over GPIO18.
- `firmware/rx/`
  - ESP-IDF receiver app (legacy path).
  - Samples digital/analog front-end, decodes Manchester stream, parses frames, checks CRC.
- `host/`
  - Python serial link, GUI, logging, and protocol helpers.
- `docs/`
  - Hardware, run guide, validation/test procedure.

## 2) Frame Format

Raw byte frame before Manchester encoding:

1. Preamble: 8 bytes of `0x55`
2. Sync word: `0xD3 0x91`
3. Header:
   - `seq` (1 byte)
   - `flags` (1 byte)
   - `length` (1 byte)
4. Payload: `length` bytes (max 200)
5. CRC16-CCITT over `seq|flags|length|payload`

This frame is converted to Manchester symbols for OOK transmission.

## 3) TX Firmware Internals

Primary (Arduino) file: `firmware/arduino/tx/tx.ino`

Legacy ESP-IDF file: `firmware/tx/main/main.c`

- UART command parser accepts:
  - `BITRATE <value>` where value in 500..10000
  - `SEND <hex_payload>`
  - `PATTERN START`
  - `PATTERN STOP`
- `send_payload()` builds frame using shared `protocol.h`.
- `tx_send_raw_bytes()` performs Manchester serialization and emits 38 kHz carrier bursts on GPIO18.
- Duty-cycle guardrail is implemented via minimum half-bit delay.

## 4) RX Firmware Internals

Main files:
- Primary Arduino: `firmware/arduino/rx/rx.ino`
- Legacy ESP-IDF: `firmware/rx/main/main.c`, `firmware/rx/main/rx_frontend.c`

Core functions:
- `rx_frontend_read_level()`
  - VS1838B digital mode: reads active-low output on GPIO14 and maps to logic-mark.
- RX parser state machine:
  - `ST_SEARCH_PREAMBLE`
  - `ST_SYNC1`
  - `ST_SYNC2`
  - `ST_HEADER`
  - `ST_PAYLOAD`
  - `ST_CRC`
- Telemetry output line format:
  - `RX seq=<n> len=<n> crc=<0|1> ber_ppm=<n> rssi=<n> hex=<payload_hex> eye=<comma_values>`

## 5) Python Host Internals

- `host/gui_app.py`
  - Connects to TX and RX serial ports.
  - Sends control commands to both nodes.
  - Plots RSSI trend, BER trend, and eye-like overlays.
- `host/serial_link.py`
  - Threaded line-based serial transport.
- `host/protocol.py`
  - Host-side frame and Manchester utilities.
- `host/logger.py`
  - CSV logging per session under `logs/`.

## 6) End-to-End Flow

1. GUI sends `BITRATE` and pattern/send commands to TX.
2. TX encodes and emits optical signal.
3. RX front-end converts 38 kHz-burst optical signal to digital logic samples.
4. RX decodes and validates frame integrity (CRC).
5. RX emits telemetry over serial.
6. GUI parses telemetry, updates graphs, and logs CSV.

## 7) Upgrade Points

Fast follow upgrades without major rewrites:
- Enable Hamming(7,4) for payload coding.
- Add interleaving and stronger FEC.
- Replace GPIO bit-bang with RMT-optimized waveform generation.
