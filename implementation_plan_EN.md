# Engine Simulator 2.0 - Implementation Plan

The **Engine Simulator 2.0** project aims to build an ESP32-based ECU simulator, serial-compatible
with ArduStim, but enriched with physical simulation of engine parameters (MAP, CLT, TPS-based RPM
inertia, cranking sequence with a Start/Stop button) and a hardware interface (PCB) to connect
directly to Speeduino through a 40-pin Dupont/IDC connector.

---

## User Review Required

> [!IMPORTANT]
> **Voltage Compatibility and Level Shifting:**
> - The ESP32 runs at **3.3V** for both analog inputs (ADC max 3.3V) and GPIOs.
> - Speeduino runs at **5V**.
> - The potentiometers (TPS, IAT, CLT, O2) are powered at 5V for Speeduino. To let the ESP32 read
>   the TPS without damaging its ADC, a resistive divider or a buffer on the PCB is used.
> - The ESP32 outputs (Crank, Cam, Cam2) must pass through a 5V level shifter before reaching Speeduino.
> - The Speeduino inputs read by the ESP32 (e.g. the 5V cooling-fan command) pass through a 3.3V level shifter.

> [!TIP]
> **Analog Generation (CLT and MAP):**
> The ESP32 has 2 built-in 8-bit DAC channels (GPIO25 and GPIO26). We use them to generate the MAP
> and CLT analog voltages (which are then translated to 0-5V through op-amps on the PCB if Speeduino
> requires a 0-5V range, since the ESP32 DACs output 0-3.3V).

---

## Proposed Changes

We will create the folder `C:\Users\nicola\Desktop\Engine Simulator 2.0` with the following file structure:

### Component 1: ESP32 Firmware
We will adapt ArduStim's trigger wheel generation logic to use the ESP32 hardware timers (via the
ESP32 Arduino Core `esp32-hal-timer.h` APIs), while implementing the physical simulation routines
in the background.

#### [NEW] [Engine_Simulator_2_0.ino](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/Engine_Simulator_2_0/Engine_Simulator_2_0.ino)
Contains the application entry point, the configuration of the ESP32 hardware timers for the trigger
wheel, and the main loop that handles:
- Start/Stop button reading (with debouncing and engine state machine: `OFF`, `CRANKING`, `RUNNING`, `STOPPING`).
- TPS reading (ESP32 ADC) and dynamic computation of the current RPM by applying inertia laws (simulated acceleration and deceleration).
- Computation and update of the DAC output for **MAP** (based on RPM, TPS, RPM rate of change).
- Computation and update of the DAC output for **CLT** (based on run time, RPM, fan state read from the ECU).
- Serial communication handling (ArduStim commands to change the trigger wheel or receive configurations).

> **[!IMPORTANT] Fix verified on hardware (2026-09):** inside the timer ISR `onCrankTimer()`,
> using `digitalWrite()` does NOT update the GPIO pins on ESP32 Arduino Core v3.3.5 (the Crank/Cam
> signals stayed at 0V even with the engine running). Fixed by replacing `digitalWrite` with
> **direct GPIO register writes** (`GPIO.out_w1ts` for HIGH, `GPIO.out_w1tc` for LOW), which always
> work inside an ISR. Added the includes `soc/gpio_struct.h` and `soc/gpio_reg.h`. After this patch
> the Crank/Cam signal switches correctly (average voltage ~1.6V = 3.3V square wave at ~50% duty)
> and Speeduino/Mega2560 reads the RPM.

#### [NEW] [wheel_defs.h](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/Engine_Simulator_2_0/wheel_defs.h)
The trigger wheel definitions imported from ArduStim (e.g. 60-2, 36-1, etc.) adapted for the ESP32.

#### [NEW] [comms.h](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/Engine_Simulator_2_0/comms.h) & [comms.cpp](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/Engine_Simulator_2_0/comms.cpp)
Serial communication module compatible with the ArduStim interface, to allow pattern selection and
configuration from a PC.

---

### Component 2: Hardware Documentation & Repository
#### [NEW] [README.md](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/README.md)
Complete documentation for GitHub, containing:
- Project description.
- Hardware block diagram (12V power -> 5V and 3.3V regulators, level shifters, 0-3.3V DACs amplified to 0-5V for MAP and CLT).
- 40-pin Dupont/IDC connector pinout for the Speeduino interface.
- Usage and calibration guide.

#### [NEW] [dupont_pinout.md](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/doc/dupont_pinout_EN.md)
Detailed table of the connections between the ESP32, the level shifters and the Speeduino 40-pin
Dupont/IDC connector.

---

## Simulated Physical Logics

### 1. RPM Inertia from TPS
- When the engine is running, the target speed is proportional to the TPS opening.
- The actual RPM follows the target with a delay controlled by an inertia coefficient (simulating the flywheel mass):
  $$\text{RPM}_{\text{curr}} = \text{RPM}_{\text{curr}} + (\text{RPM}_{\text{target}} - \text{RPM}_{\text{curr}}) \times K_{\text{inertia}}$$

### 2. Manifold Pressure (MAP)
- Engine off: MAP = atmospheric pressure (~4.0V - 4.5V at the sensor, depending on the simulated MAP sensor).
- Idle (low TPS, stable RPM): MAP drops sharply (strong vacuum, ~1.0V - 1.5V).
- Acceleration (high TPS, rising RPM): MAP rises quickly toward atmospheric pressure (~4.0V) before the RPM fully increases.
- Deceleration (closed TPS, high RPM): MAP drops to its historical minimum (~0.5V - 0.8V).

### 3. Coolant Temperature (CLT)
- Engine off for a long time: CLT = ambient temperature.
- Engine running: CLT rises gradually toward an operating temperature (~90°C). The heating rate depends on RPM.
- If CLT exceeds the fan threshold (~95°C), Speeduino activates the Fan output. The ESP32 detects this input and increases the cooling coefficient, causing the temperature to fall or stabilize.
- Engine off: CLT slowly falls back toward ambient temperature following Newton's cooling law.

---

## PHASE 2 - Input Level Shifters + Injector/Coil Timing Monitoring

> [!NOTE]
> This section describes the planned implementation. At the time it was written, **the PWM monitoring
> of injectors/coils was not yet implemented** (the 8-9 inputs were not yet handled). The base firmware
> (Crank/Cam generation + engine model) was already working and verified on Speeduino.

### Objective

Monitor the **PWM output signals of the Speeduino** (4 injectors + 4 coils + fan) to measure
**injector open times (ms) and ignition advance (°)**. This allows real-time verification that the
ECU and its corrections (lambda, CLT, etc.) actually modify the parameters.

### 1. Data lane: the Speeduino signals to read (IDC 40 pin V0.4)

| # | Signal | IDC Pin | Nature |
|:-:|:-------|:-------:|:-------|
| 1 | Injector 1 | 1 | PWM low-side, active low |
| 2 | Injector 2 | 2 | PWM low-side, active low |
| 3 | Injector 3 | 3 | PWM low-side, active low |
| 4 | Injector 4 | 5 | PWM low-side, active low |
| 5 | Ignition 1 | 7 | PWM low-side, active low |
| 6 | Ignition 2 | 34 | PWM low-side, active low |
| 7 | Ignition 3 | 33 | PWM low-side, active low |
| 8 | Ignition 4 | 8 | PWM low-side, active low |
| 9 | Fan (ECU feedback) | — | Fan command signal, read via CD4050-A |

These signals are at **5V/12V Speeduino logic**. To read them with the ESP32 (3.3V), a
**protection network + buffer** (CD4050BE) is needed on each input.

### 2. Level shifter: component and rationale

**Reason:** the Speeduino injector/coil outputs are **low-side**: the IDC pin swings between ~0V (ON)
and up to **12V** (battery/Vdrive) when OFF. Reading this pin directly exceeds the ESP32's 3.3V and
would damage both the GPIO and any 5V level shifter (Vmax ~5.5V). ESP32 DevKitC boards are **not
5V/12V tolerant** -> dedicated protection is required.

**Chosen component: 2x CD4050BE** (Hex Non-Inverting Buffer, 6 channels each).

| Property | Value |
|:---------|:------|
| Input tolerance | **VDD max 18V** -> withstands the 12V Speeduino signals without damage |
| Channels | 6 per chip (8 needed => **2 chips**) |
| Supply (decision) | **VDD = 3.3V**, GND shared with the ESP32 |
| Output | VDD level = **3.3V**, directly compatible with the ESP32 GPIO |
| Direction | Unidirectional (input -> output), perfect for reading |

**Wiring (2 CD4050BE chips, 1 chip per system):**

```
Speeduino IDC (injectors/coils/fan, 0-12V)
        |   (each signal on one input)
        ▼
  [ CD4050BE-A  = IGNITION  (4 coils: IGN1-4 + FAN) ]
  [ CD4050BE-B  = INJECTION (4 injectors: INJ1-4)    ]
        |  (VDD=3.3V, GND=ESP32 GND)
        ▼
  ESP32 GPIO (9 inputs, 3.3V level)
```

> Final verified distribution: **CD4050BE-A = IGNITION** (buffers 9→10, 11→12, 14→15, 3→2 + FAN 7→6),
> **CD4050BE-B = INJECTION** (buffers 5→4, 7→6, 9→10, 11→12). See sections 2.3-2.4 for the pin-by-pin details.

> The 3.3V VDD makes the CD4050 output already logic-compatible with the ESP32, eliminating further
> dividers on the monitoring signal paths.

#### 2.1 Input protection network (for each of the 8 channels)

Before the CD4050 input, a protection network limits the current and **clamps the voltage** below the
logic threshold. The CD4050 itself withstands 12V, but the network protects against wiring errors/spikes
and keeps the input at a clean level.

**Per-channel schematic (TH - Through Hole):**

```
Speeduino IDC ──[R 2.2k 1/4W]──┬──────> CD4050 IN
 (injector/coil)                │
                           Zener 3.6V
                           (BZX55C3V6, DO-35)
                                │
                               GND
```

| Component | Value | TH Package | Role |
|:----------|:------|:-----------|:-----|
| Series R | **2.2kΩ** 1/4W | TH | Limits the current into the Zener and the CD4050 input |
| Zener | **3.6V** (`BZX55C3V6`) | **DO-35 (TH)** | Clamps the overvoltage just above 3.3V, protects the input and GPIO |

**Availability note (TH):** the **BZX55C3V6** comes in a **DO-35 (TH**, 2-pin, 0.5W) package — easy to
source. Equivalent TH values if the 3.6V is unavailable:

| Alternative | Voltage | TH Package |
|:------------|:-------:|:-----------|
| 1N5226B | 3.3V | DO-35 |
| BZX55C3V6 | 3.6V | DO-35 (chosen) |
| 1N5228B | 3.9V | DO-35 |

> The **3.6V Zener** (and not 5.1V) was chosen because clamping just above 3.3V really protects the
> input, given that the CD4050 is powered at VDD=3.3V. A 5.1V clamp would be redundant/partial. Schottky
> diodes (BAT85/1N5819) are not Zeners and do not provide a threshold clamp: discarded.

**Protection network quantity:** 9 channels => 9x R2.2k + 9x BZX55C3V6 (all TH).

#### 2.2 KiCad symbol for the CD4050BE

> The CD4050B is **not present** in the standard KiCad 10 libraries (the generic "Hex Buffer" symbol
> has a different pinout and must be discarded). The Digi-Key library exists but uses the legacy `.lib`
> format, poorly compatible with KiCad 9/10 for a single component.

**Recommended solution: create a custom symbol** in a project library (`.kicad_sym`) with the pinout
confirmed by the TI datasheet (CD4049UB/CD4050B).

**CD4050BE pinout (PDIP-16, from the TI datasheet):**

| Pin | Function | Pin | Function |
|:---:|:---------|:---:|:---------|
| 1 | VDD | 16 | **NC** (not connected) |
| 2 | OUT (G) | 15 | OUT (L) |
| 3 | IN (A) | 14 | IN (F) |
| 4 | OUT (H) | 13 | **NC** (not connected) |
| 5 | IN (B) | 12 | OUT (K) |
| 6 | OUT (I) | 11 | IN (E) |
| 7 | IN (C) | 10 | OUT (J) |
| 8 | VSS (GND) | 9 | IN (D) |

> **Important (from the TI datasheet, SCHS046):** pins **13 and 16 are NC** (internally unconnected).
> The **6 valid buffers** are on the pairs (IN->OUT): **(3→2), (5→4), (7→6), (9→10), (11→12), (14→15)**.
> Inputs: A=3, B=5, C=7, D=9, E=11, F=14 · Outputs: G=2, H=4, I=6, J=10, K=12, L=15.
> **Do NOT use pins 13 and 16 as input/output.**

**Key CD4050B data (from the datasheet):**
- **Non-inverting** buffer, **6 channels** (hex)
- **Pins 13 and 16 = NC** — use only the 6 valid pairs above
- VDD range **3-18V** (withstands 12V)
- Input current max 1µA, TH tolerant
- TH package: **PDIP-16** -> footprint `Package_DIP:DIP-16_W7.62mm`

**KiCad procedure:**
1. `File → New → Symbol Library` -> create `engine_sim.kicad_sym` in the project folder
2. `File → New Symbol` -> name it `CD4050BE`
3. Add the pins according to the table above (6 valid pairs + VDD pin1 + VSS pin8; pins 13/16 NC)
4. Assign footprint `Package_DIP:DIP-16_W7.62mm`
5. Save

**Reference footprint (for the TH CD4050BE):** `Package_DIP:DIP-16_W7.62mm`

### 2.3 Assignment of the 2 chips (CD4050BE-A and CD4050BE-B)

The 2 CD4050BE chips are split **by system**: **chip A = Ignition (4 coils)**, **chip B = Injection
(4 injectors)**. Each chip uses only the valid pairs (the 6 from the TI datasheet); pins 13 and 16 (NC)
are never used. Both chips: **VDD(pin1)=3.3V**, **VSS(pin8)=GND**.

**CD4050BE-A (IGNITION + FAN — 4 coils + fan):**

| Signal | IDC Pin | IN CD4050-A | OUT CD4050-A | ESP32 GPIO | DevKitC Pin |
|:-------|:-------:|:-----------:|:------------:|:----------:|:-----------:|
| IGN1 | 7 | 9 | 10 | GPIO 19 | 31 |
| IGN2 | 34 | 11 | 12 | GPIO 21 | 33 |
| IGN3 | 33 | 14 | 15 | GPIO 22 | 36 |
| IGN4 | 8 | 3 | 2 | GPIO 23 | 37 |
| FAN | — | 7 | 6 | GPIO 27 | 11 |

**CD4050BE-B (INJECTION — 4 injectors):**

| Injector | IDC Pin | IN CD4050-B | OUT CD4050-B | ESP32 GPIO | DevKitC Pin |
|:---------|:-------:|:-----------:|:------------:|:----------:|:-----------:|
| INJ1 | 1 | 5 | 4 | GPIO 15 | 23 |
| INJ2 | 2 | 7 | 6 | GPIO 16 | 27 |
| INJ3 | 3 | 9 | 10 | GPIO 17 | 28 |
| INJ4 | 5 | 11 | 12 | GPIO 18 | 30 |

> Each input (IN) passes through the **protection network** (R 2.2k + 3.6V Zener). **Chip A** uses 5 of
> the 6 valid buffers (4 coils + Fan), **chip B** uses 4. Pins 13 and 16 are NC on both, never used;
> the spare buffers stay unconnected.

#### 2.4 Connection table per system (Ignition / Injection)

Consolidated view organized **by system** — **1 chip per system** (all values are physical CD4050 pins
from the TI datasheet):

**CD4050BE-A = IGNITION + FAN (4 coils + fan):**

| Signal | IDC Pin | IN CD4050-A | OUT CD4050-A | ESP32 GPIO | DevKitC Pin |
|:-------|:-------:|:-----------:|:------------:|:----------:|:-----------:|
| IGN1 | 7 | 9 | 10 | GPIO 19 | 31 |
| IGN2 | 34 | 11 | 12 | GPIO 21 | 33 |
| IGN3 | 33 | 14 | 15 | GPIO 22 | 36 |
| IGN4 | 8 | 3 | 2 | GPIO 23 | 37 |
| FAN | — | 7 | 6 | GPIO 27 | 11 |

**CD4050BE-B = INJECTION (4 injectors):**

| Injector | IDC Pin | IN CD4050-B | OUT CD4050-B | ESP32 GPIO | DevKitC Pin |
|:---------|:-------:|:-----------:|:------------:|:----------:|:-----------:|
| INJ1 | 1 | 5 | 4 | GPIO 15 | 23 |
| INJ2 | 2 | 7 | 6 | GPIO 16 | 27 |
| INJ3 | 3 | 9 | 10 | GPIO 17 | 28 |
| INJ4 | 5 | 11 | 12 | GPIO 18 | 30 |

> **Chip A** uses 5 buffers (4 coils + Fan), **chip B** uses 4. Pins 13 and 16 are NC on both, never
> used. VDD(pin1)=3.3V, VSS(pin8)=GND on both.

### 3. Identification of the 9 ESP32 inputs (GPIO)

The current firmware uses: GPIO 4, 34, 27, 25, 26, 12, 13, 14 (all others free).
PWM monitoring needs GPIOs **with interrupt/input capture** (all ESP32 GPIOs support interrupts).
Proposed choice (free RTC channel, no conflict with the DAC/ADC in use):

| Function | GPIO | DevKitC Pin | Notes |
|:---------|:----:|:-----------:|:------|
| Injector 1 | **GPIO 15** | PIN 23 | INPUT_PULLUP |
| Injector 2 | **GPIO 16** | PIN 27 | INPUT_PULLUP |
| Injector 3 | **GPIO 17** | PIN 28 | INPUT_PULLUP |
| Injector 4 | **GPIO 18** | PIN 30 | INPUT_PULLUP |
| Ignition 1 | **GPIO 19** | PIN 31 | INPUT_PULLUP |
| Ignition 2 | **GPIO 21** | PIN 33 | INPUT_PULLUP |
| Ignition 3 | **GPIO 22** | PIN 36 | INPUT_PULLUP |
| Ignition 4 | **GPIO 23** | PIN 37 | INPUT_PULLUP |
| Fan (via CD4050-A) | **GPIO 27** | PIN 11 | INPUT_PULLUP |

> These GPIOs do not conflict with the already-assigned I/O pins (TPS, MAP, CLT, Crank, Cam1, Cam2,
> Fan, Start/Stop) — GPIO 27/Fan is an output of CD4050-A (buffer 7→6) that already falls among the
> GPIOs used by the firmware.
>
> **Physical DevKitC pin mapping note (verified from the board):** GPIO15→PIN23, GPIO16→PIN27,
> GPIO17→PIN28, GPIO18→PIN30, GPIO19→PIN31, GPIO21→PIN33, GPIO22→PIN36, GPIO23→PIN37, GPIO27→PIN11.
> The PIN numbers in all tables refer to this mapping (not to the standard pinout of other DevKitC boards).

### 4. Timing algorithm to implement

An **"input capture on ISR + timestamp via micros()"** technique is used to measure each channel's
parameters without blocking the main loop (which must stay low-latency for the engine model).

#### 4.1 Injector time measurement (duty / on-time)
For each injector channel, both the **rising and falling** edges are detected and, in the ISR:
- `onTime_us[ch]` = duration of the ACTIVE pulse (injector opening) in microseconds -> converted to ms.
- `period_us[ch]` = full cycle period (between two consecutive injections).
- **Injection frequency** = `1 / period` (doubles in semi-sequential/batch mode).

This directly gives the **injection time** (in ms) the ECU is applying, and makes it possible to see
the corrections (cold-CLT enrichment, acceleration enrichment, etc.).

#### 4.2 Coil advance measurement (spark timing)
For the coil, what is needed is not just the duty but the **temporal position of the pulse relative
to the Crank reference**. Since the ESP32 generates the trigger wheel (and is therefore the "master",
knowing the engine revolution), it can:
- Detect the coil pulse on the ignition channel.
- Compare the timestamp with the last **Crank** edge (already generated locally).
- Compute the angle in degrees = `(Δt / rev_period) × 360°`.

**Result:** ignition advance in **degrees BTDC**, the key parameter the ECU modifies with MAP/RPM
(advance table).

#### 4.3 Which core/priority to run on
- The 8 input-capture interrupts run in the ISR (trigger instant, only `micros()` and store into a
  volatile structure). **Phase 1 (Crank/Cam generation)** stays unchanged on the timer.
- The **computation** of the final values (ms, degrees, frequencies) happens in the loop (low priority)
  and is only **exposed** in shared memory.
- The **WiFi/web server** (later phase) and the serial interface read the computed values -> no timing conflict.

#### 4.4 Data structures (to add to `globals.h`)
```cpp
struct injectorChannel {
  volatile uint32_t onTimeUs;   // open duration (us)
  volatile uint32_t periodUs;   // cycle period (us)
  volatile uint32_t lastRise;   // rising edge timestamp (micros)
  volatile uint32_t lastFall;   // falling edge timestamp (micros)
  bool active;                  // output active?
};

struct ignitionChannel {
  volatile uint32_t sparkTimeUs;   // coil pulse duration
  volatile uint16_t advanceDeg;    // advance in degrees BTDC
  bool active;
};

extern injectorChannel injectors[4];
extern ignitionChannel ignitions[4];
```

#### 4.5 Interrupt handler (conceptual example, NOT yet in the code)
```cpp
void IRAM_ATTR injISR() {
  uint32_t now = micros();
  bool pinState = digitalRead(injPin);
  if (pinState == LOW) {            // active low: falling edge = opening
    inj.onTimeUs = now - inj.lastRise;
    inj.periodUs = now - inj.lastRise + inj.lastPeriod...; // handling fractions
    inj.lastRise = now;
  }
}
```

### 5. Stubs / placeholders in the current code (not to be modified yet)
- No software changes until the plan is approved and the development sequence is defined.
- The new files/symbols will be introduced in a later tranche without breaking the current compilation (which is verified).

### 6. Planned verification (Phase 2)
1. **Oscilloscope on the 3 level shifters**: verify that the 5V → 3.3V signals are clean (no undershoot).
2. **Timing comparison**: compare the injection ms measured by the firmware with those shown by TunerStudio.
3. **Advance verification**: compare the computed degrees with Speeduino's spark advance table.
4. **Stability**: ensure the 8 ISRs do not interfere with Crank/Cam generation (high-RPM test).

---

## Verification Plan

### Automated/Unit Tests
Since there is no automatic hardware test bench integrated into the local computer, the analog readings
and physical behaviors are simulated through a software simulation module that can be enabled via the
serial console.

### Manual Verification
1. **Oscilloscope or Logic Analyzer analysis:** verify correct generation of the Crank and Cam signals on the selected GPIOs.
2. **Serial test:** connect via a serial terminal (e.g. Arduino Serial Monitor) at 115200 baud to validate the ArduStim command exchange and check the responses.
3. **DAC verification:** read the MAP and CLT analog outputs in response to TPS (simulated) or Start button changes.
4. **[x] Crank/Cam test on Speeduino (done):** complete — the firmware generates Crank (GPIO12)/Cam1 (GPIO13)/Cam2 (GPIO14) at 3.3V and Speeduino (Mega2560) reads the engine speed correctly. It required the GPIO-register fix inside the ISR (see Component 1). The Start/Stop button (GPIO4) starts the engine up to ~850 rpm idle; on the Crank/Cam outputs a ~1.6V average is measured (3.3V square wave, ~50% duty).
