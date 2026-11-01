# Hardware Wiring Notes (ESP32 + IR FSO)

This guide gives practical wiring for a one-day prototype with VS1838B digital IR receiver module.

## 1) TX Node Wiring (required)

### 1.1 Parts

- ESP32 dev board
- IR LED (850/940 nm)
- NPN transistor (2N2222) or logic-level N-MOSFET
- Base/gate resistor (220R to 1k)
- LED series resistor (value from current calculation)
- Optional capacitor near driver supply (100 nF + 10 uF)

### 1.2 Connection Steps

1. Connect ESP32 `GPIO18` to transistor base/gate through resistor.
2. Build low-side switch:
	 - LED anode to supply rail through series resistor.
	 - LED cathode to transistor collector/drain.
	 - transistor emitter/source to GND.
3. Connect ESP32 GND to driver supply GND (common reference is mandatory).
4. Place decoupling capacitor close to LED driver branch.
5. Use the updated TX firmware which emits ~38 kHz carrier bursts for logic-mark intervals.

### 1.3 LED Resistor Sizing

Use conservative startup current:

- For NPN path approximate:
	- `R = (Vsup - Vf_led - Vce_sat) / I_led`
- For MOSFET path approximate:
	- `R = (Vsup - Vf_led) / I_led`

Example at 5V, IR LED Vf=1.4V, target pulse current 40 mA:
- `R ~= (5 - 1.4 - 0.2) / 0.04 = 85 ohm`
- choose standard `82R` or `91R` and verify thermals.

Always stay below datasheet pulse and average current limits.

## 2) RX Node with VS1838B Module

VS1838B provides a demodulated digital output and is active LOW when valid carrier is present.

### 2.1 Parts

- VS1838B module (3-pin: VCC, GND, OUT)
- ESP32 dev board

### 2.2 Connection Steps

1. Connect VS1838B `VCC` -> ESP32 `3V3`.
2. Connect VS1838B `GND` -> ESP32 `GND`.
3. Connect VS1838B `OUT` -> ESP32 `GPIO14`.
4. Keep wires short and away from TX switching wires.
5. Place module facing TX beam axis for alignment.

### 2.3 Important Limitation

VS1838B is designed for ~38 kHz modulated IR and not raw DC/baseband optical levels.
For reliable use, TX should emit a 38 kHz carrier burst for logic-mark intervals.

## 4) ESP32 Pin Map (current firmware)

- TX node
	- `GPIO18`: optical transmit data output
- RX node
	- `GPIO14`: VS1838B digital receive input (active-low)
- Both nodes
	- `UART0` over USB: command + telemetry

## 5) Physical Setup and Alignment

1. Start at 1 m LOS.
2. Use fixed mounts/books/tripods to reduce shaking.
3. Start TX pattern mode and align for stable `crc=1` frames.
4. Increase distance to 3 m, then 5 m.
5. If unstable:
	 - narrow beam and reduce ambient light
	 - reduce bitrate to 1000 or 500 for VS1838B

## 6) Safety Checklist

- Never exceed LED pulse current/temperature limits.
- Avoid direct eye exposure to high-intensity IR source.
- Power down before rewiring.
- Confirm no short circuits before USB power-up.
