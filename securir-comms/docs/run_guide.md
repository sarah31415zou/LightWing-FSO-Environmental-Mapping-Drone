# Setup and Run Guide (Step-by-Step)

Use this guide for first-time setup, firmware flashing, and live demo execution.

## 0) Prerequisites

Hardware minimum:
- 2x ESP32 dev boards (one TX, one RX)
- 1x IR LED transmitter + driver transistor/MOSFET + resistor
- 1x VS1838B IR receiver module
- USB data cables for both ESP32 boards

Software minimum:
- Arduino IDE 2.x with ESP32 boards package
- Python 3.11+ (3.13 is fine)
- Windows PowerShell

Firmware source for Arduino IDE:
- TX sketch: `firmware/arduino/tx/tx.ino`
- RX sketch: `firmware/arduino/rx/rx.ino`

## 1) Python Environment Setup

From repository root:

Option A (pip):
```powershell
D:/devtools/Python313/python.exe -m pip install -r requirements.txt
```

Option B (uv):
```powershell
uv pip install -r requirements.txt
```

Quick verify:
```powershell
D:/devtools/Python313/python.exe -c "import tkinter, serial, numpy, matplotlib; print('python deps ok')"
```

## 2) Flash Firmware (Arduino IDE, recommended)

### 2.1 Install ESP32 board support in Arduino IDE

1. Open Arduino IDE.
2. Go to **File > Preferences**.
3. In Additional Boards Manager URLs, add:
  - `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
4. Go to **Tools > Board > Boards Manager**.
5. Search `esp32` and install **esp32 by Espressif Systems**.

### 2.2 Flash TX board

1. Open sketch `firmware/arduino/tx/tx.ino`.
2. Select:
  - **Board**: `ESP32 Dev Module` (works for ESP32-WROOM 38-pin)
  - **Port**: TX COM port (example COM3)
  - **Upload Speed**: 921600 (or 115200 if unstable)
3. Click **Upload**.
4. Open Serial Monitor at `115200` baud and verify boot line:
  - `FSO_TX ready bitrate=2000 carrier=38kHz`

### 2.3 Flash RX board

1. Open another Arduino IDE window or close TX sketch and open `firmware/arduino/rx/rx.ino`.
2. Select:
  - **Board**: `ESP32 Dev Module`
  - **Port**: RX COM port (example COM4)
3. Click **Upload**.
4. Open Serial Monitor at `115200` baud and verify:
  - `FSO_RX ready bitrate=2000 PIN=14 (VS1838B active-low)`

If upload fails:
- Hold `BOOT` during the first seconds of upload.
- Try lower upload speed.
- Replace cable with a known data cable.

## 3) Build and Flash Firmware (ESP-IDF legacy path)

Use this only if you prefer ESP-IDF over Arduino IDE.

### 3.1 TX Board

```powershell
cd firmware/tx
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

Expected boot line:
- `FSO_TX ready bitrate=5000`

### 3.2 RX Board

Open another terminal:

```powershell
cd firmware/rx
idf.py set-target esp32
idf.py build
idf.py -p COM4 flash monitor
```

Expected boot line:
- `FSO_RX ready bitrate=5000 mode=DIGITAL`

If your ports differ, replace COM3/COM4 accordingly.

## 4) Basic Serial Command Check (Optional but Recommended)

In TX monitor terminal, type:
- `BITRATE 2000`
- `PATTERN START`
- `PATTERN STOP`

In RX monitor terminal, type:
- `BITRATE 2000`
- `MODE DIGITAL`

Expected responses start with `OK`.

## 5) Run GUI

From repository root:

```powershell
D:/devtools/Python313/python.exe host/gui_app.py
```

Inside GUI:
1. Set TX port and RX port.
2. Set bitrate to `2000` (try 1000 or 500 if unstable).
3. Click **Connect**.
4. Click **Start Pattern**.
5. Observe RX log and graphs updating.

## 6) First Optical Bring-Up Procedure

1. Start with TX and RX at ~1 m LOS.
2. Align beam while pattern runs.
3. Wait for recurring `RX ... crc=1` lines.
4. Increase distance to 3 m, then 5 m.
5. If unstable:
  - reduce bitrate to 1000 or 500
  - improve alignment
  - reduce ambient light and improve optical shielding

## 7) Command Reference

Commands sent to TX:
- `BITRATE <500..10000>`
- `SEND <hex>` e.g., `SEND 48454C4C4F`
- `PATTERN START`
- `PATTERN STOP`

Commands sent to RX:
- `BITRATE <500..10000>`
- `MODE DIGITAL`

## 8) Typical Demo Script (5–8 min)

1. Show architecture and hardware.
2. Show TX/RX boot lines.
3. Run GUI and connect.
4. Start pattern, show BER/RSSI/eye-like plot.
5. Move from 1 m to 5 m and show stable decode.
6. Misalign briefly to show BER rise, then recover.
7. Stop pattern and send custom payload with **Send Once**.

## 9) Troubleshooting

No serial connection:
- Check COM ports in Device Manager.
- Ensure monitor terminal is closed before GUI opens same port.

No RX frames:
- Confirm common GND and correct receiver output pin.
- Reduce distance and realign.
- Lower bitrate to 2000.

High CRC failure:
- Keep `MODE DIGITAL` (VS1838B is digital only).
- Add optical shielding/tube to receiver.
- Reduce ambient light interference.
- Lower bitrate to 1000 or 500.

GUI opens but no plots:
- Confirm RX lines contain `RX seq=...` format.
- Check that `matplotlib` backend loads correctly.

## 10) What “Validated” Means for Day-1

Minimum success:
- 2 kbps preferred (fallback 1 kbps or 500 bps) at 5 m LOS indoor.
- Continuous frame reception with mostly `crc=1`.
- BER trend and RSSI trend visible in GUI.
- CSV session log generated in `logs/`.
