# Bench/Lab Validation Plan

Use this as your demo-day execution script.

## 1) Pre-Flight Checklist

Before powering:
- Confirm TX and RX have common-ground within each node wiring.
- Verify TX pin is `GPIO18`, VS1838B output is on `GPIO14`.
- Ensure LED series resistor is installed.
- Ensure USB cables are data-capable.

Software pre-check:
- Firmware flashed on both boards.
- Python dependencies installed.
- GUI launches without errors.

## 2) Bring-Up Sequence

1. Place TX and RX at 1 m LOS.
2. Open GUI and connect both COM ports.
3. Set bitrate `2000`.
4. Start pattern.
5. Verify RX stream contains repetitive lines:
	- `RX seq=... crc=1 ...`
6. Let run 60 seconds.

Pass criteria:
- Continuous telemetry updates.
- BER trend stable (not saturating high values).
- No repeated disconnect/reconnect events.

## 3) Distance Sweep (Core Demonstration)

Repeat at 1 m, 3 m, 5 m.

At each distance:
1. Run pattern for 60 s.
2. Note average RSSI trend level.
3. Note BER proxy (`ber_ppm`) trend.
4. Save/label CSV log segment.

Template table:

| Distance | Bitrate | RX Mode | Avg RSSI | BER ppm trend | Result |
|---|---:|---|---:|---:|---|
| 1 m | 2000 | DIGITAL |  |  | PASS/FAIL |
| 3 m | 2000 | DIGITAL |  |  | PASS/FAIL |
| 5 m | 2000 | DIGITAL |  |  | PASS/FAIL |

## 4) Stress/Recovery Validation

### 4.1 Misalignment Recovery

1. Deliberately offset beam.
2. Observe BER rise / eye collapse.
3. Re-align and confirm BER recovers.

### 4.2 Ambient Light Stress

1. Increase room lighting or expose receiver to brighter background.
2. Observe BER and RSSI behavior.
3. Apply mitigation in order:
	- add shielding/tube
	- keep strict LOS alignment
	- lower bitrate to 2000

## 5) Fallback Decision Tree (if 5 m @ 5 kbps is unstable)

1. Keep 5 m, reduce bitrate to 2000.
2. Reduce bitrate to 1000 or 500.
3. Improve alignment and reduce ambient light.
4. If still unstable, migrate TX to 38kHz-carrier burst signaling optimized for VS1838B.

Demo still considered successful if:
- stable 5 m LOS at 2 kbps with clear telemetry and GUI plots,
- and you can show controlled degradation/recovery behavior.

## 6) Logging and Evidence Pack

Collect for presentation:
- Screenshot of GUI with RSSI, BER, eye-like plot.
- CSV log from `logs/`.
- Short video of 1 m -> 5 m transition.
- Final table with settings and outcomes.

## 7) Final Acceptance Criteria (Day-1)

Minimum acceptance:
- End-to-end TX->FSO->RX->GUI pipeline works.
- 5 m LOS achieved indoors at 2 kbps preferred or 1 kbps/500 bps fallback.
- Most frames valid (`crc=1`) during stable alignment.
- BER and RSSI trends visible and interpretable in GUI.
