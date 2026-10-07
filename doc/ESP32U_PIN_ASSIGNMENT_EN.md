# ESP32U Pin Assignment — Engine Simulator 2.0

## Overview

This document specifies the pin configuration for the **ESP32-WROOM-32U** microcontroller module used in the Engine Simulator 2.0 project.

The ESP32-WROOM-32U features a U.FL antenna connector for external RF antenna integration while maintaining the standard 38-pin form factor and pinout of the ESP32 WROOM family.

---

## Functional Responsibilities

The ESP32 module is responsible for:

- **Signal generation** — Produces digital trigger signals for crankshaft position (CKP), camshaft position bank 1 (CMP1), and camshaft position bank 2 (CMP2)
- **Analog simulation** — Generates MAP (manifold absolute pressure) and CLT (coolant temperature) sensor voltages
- **Input monitoring** — Reads throttle position sensor (TPS) for RPM target determination and fan feedback state
- **Control interface** — Manages engine start/stop button input

**Note:** IAT (intake air temperature) and O2 (oxygen sensor) signals are controlled via external potentiometers connected directly to the Speeduino ECU, independent of the ESP32.

---

## Pin Assignment Reference

### Digital Inputs

| GPIO | Physical Pin | Signal Name | Function | Electrical Characteristics | Notes |
|:----:|:------------:|:------------|:---------|:---------------------------|:------|
| 4    | 26           | Start/Stop Button | Engine start/stop control | 3.3V logic; internal pull-up enabled | Button closes to GND when pressed |
| 27   | 12           | Fan Feedback | ECU fan activation status | 3.3V logic input | **Requires 5V→3.3V voltage divider or level shifter from Speeduino IDC Pin 15** |

### Analog Inputs

| GPIO | Physical Pin | ADC Channel | Signal Name | Function | Electrical Characteristics | Notes |
|:----:|:------------:|:-----------:|:------------|:---------|:---------------------------|:------|
| 34   | 6            | ADC1_CH6    | TPS Monitor | Throttle position feedback | 0–3.3V; 12-bit ADC (4096 levels) | **Requires 5V→3.3V resistive divider from TPS potentiometer wiper** |

### Analog Outputs

| GPIO | Physical Pin | DAC Channel | Signal Name | Function | Output Range | Conditioning | Speeduino Connection |
|:----:|:------------:|:-----------:|:------------|:---------|:--------------|:-------------|:--------------------:|
| 25   | 10           | DAC1        | MAP Voltage | Manifold absolute pressure simulation | 0–3.3V (8-bit) | Op-amp amplifier (gain ≈ 1.5×) | IDC Pin 11 (MAP Sensor) |
| 26   | 11           | DAC2        | CLT Voltage | Coolant temperature simulation | 0–3.3V (8-bit) | Op-amp amplifier (gain ≈ 1.5×) | IDC Pin 19 (CLT Sensor) |

### Digital Outputs

| GPIO | Physical Pin | Signal Name | Function | Output Level | Conditioning | Speeduino Connection |
|:----:|:------------:|:------------|:---------|:------------:|:-------------|:--------------------:|
| 12   | 14           | Crank Trigger | CKP signal generation | 3.3V CMOS | **3.3V→5V level shifter (74AHCT125)** | IDC Pin 25 (Crank Input / VR1+) |
| 13   | 16           | Cam1 Trigger | CMP1 signal generation | 3.3V CMOS | **3.3V→5V level shifter (74AHCT125)** | IDC Pin 24 (Cam Input / VR2+) |
| 14   | 13           | Cam2 Trigger | CMP2 signal generation | 3.3V CMOS | **3.3V→5V level shifter (74AHCT125)** | Optional; required for complex trigger wheels only |

### Common Signals

| Physical Pin | Signal | Purpose |
|:------------:|:-------|:--------|
| —            | GND    | Common ground return (Speeduino ↔ ESP32) |

---

## Signals Managed Outside the ESP32

The following sensor inputs are controlled via external potentiometers connected directly to the Speeduino ECU, bypassing the ESP32:

| Sensor | Speeduino IDC Pin | Signal Path |
|:-------|:-----------------:|:------------|
| IAT (Intake Air Temperature) | Pin 20 | External potentiometer → Speeduino |
| O2 (Oxygen Sensor / Lambda) | Pin 21 | External potentiometer → Speeduino |

---

## Signal Conditioning Requirements

### Voltage Level Shifting

**3.3V → 5V (Digital Outputs):**
- Devices: GPIO 12, 13, 14 (CKP and CMP signals)
- Conditioning: 74AHCT125 or equivalent level shifter
- Reason: Speeduino CKP/CMP inputs operate at 5V logic levels; ESP32 outputs are 3.3V

**5V → 3.3V (Digital & Analog Inputs):**
- Devices: GPIO 27 (Fan feedback), GPIO 34 (TPS input)
- Conditioning: Resistive voltage divider or dedicated level shifter module
- Reason: Speeduino outputs and external sensors operate at 5V; ESP32 ADC/GPIO inputs are 3.3V maximum

### Analog Amplification

**DAC Output Amplification (GPIO 25, 26):**
- Op-amp: TLV2372 (rail-to-rail) or equivalent
- Gain: Approximately 1.5×
- Input: 0–3.3V (ESP32 DAC)
- Output: 0–5V (Speeduino sensor inputs)
- Purpose: Stretch the 3.3V DAC range to match 5V Speeduino sensor input expectations for MAP and CLT

---

## Pinout Summary Table

| ESP32 GPIO | Type | Direction | Function | Conditioning | ECU / PCB Target |
|:----------:|:----:|:---------:|:---------|:-------------|:-----------------|
| 4          | D    | Input     | Start/Stop Button | Internal pull-up | Physical push button to GND |
| 12         | D    | Output    | Crank Trigger (CKP) | Level shifter (3V3→5V) | IDC Pin 25 |
| 13         | D    | Output    | Cam1 Trigger (CMP1) | Level shifter (3V3→5V) | IDC Pin 24 |
| 14         | D    | Output    | Cam2 Trigger (CMP2) | Level shifter (3V3→5V) | Optional |
| 25         | A    | Output    | MAP Sensor Output (DAC1) | Op-amp (gain ≈1.5×) | IDC Pin 11 |
| 26         | A    | Output    | CLT Sensor Output (DAC2) | Op-amp (gain ≈1.5×) | IDC Pin 19 |
| 27         | D    | Input     | Fan Feedback | Divider (5V→3V3) | IDC Pin 15 |
| 34         | A    | Input     | TPS Monitor (ADC1_CH6) | Divider (5V→3V3) | TPS potentiometer wiper |
| —          | P    | —         | Ground (GND) | —            | Common return (Speeduino & power rails) |

---

## Implementation Notes

1. **Level Shifters:** Use buffered level shifter ICs (e.g., 74AHCT125) rather than passive resistors for digital trigger outputs to maintain signal integrity and timing accuracy over CAN/wired connections.

2. **Pull-up Resistors:** GPIO 4 has an internal pull-up enabled in firmware. External pull-up is not required.

3. **ADC Filtering:** Consider adding RC low-pass filters (e.g., 10 kΩ + 100 nF) on GPIO 34 (TPS input) to suppress high-frequency noise.

4. **DAC Output Impedance:** The ESP32 DAC has significant output impedance (~1 kΩ); use a unity-gain buffer op-amp stage before the gain stage to isolate the DAC from load variations.

5. **Ground Plane:** Ensure a solid ground plane connection between the ESP32 PCB and the Speeduino ECU to minimize noise coupling on analog signals (MAP, CLT, TPS).

---

## References

- ESP32-WROOM-32U Datasheet
- Speeduino Standard 38-Pin ECU Connector (IDC) Pinout
- TLV2372 Op-amp Datasheet (signal conditioning)
- 74AHCT125 Level Shifter Datasheet (digital conditioning)
