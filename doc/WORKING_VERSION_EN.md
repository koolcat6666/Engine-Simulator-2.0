# Firmware Working Version

**Status (2026-09-04):** Phase 1 (Crank/Cam generation + engine model) confirmed WORKING on hardware.

**Status (Phase 2 + Phase 3):** injector/ignition timing (Phase 2), WiFi AP + web dashboard (Phase 3)
and the startup synchronization mode (Sync Mode Passive/Active) are implemented, compiled and
validated on hardware.

## Single source tree

The firmware is published from a single folder: **`Engine_Simulator_2_0/`** (the folder name
matches the `.ino` sketch name, which is required by the Arduino IDE / arduino-cli).

Main files:

- `Engine_Simulator_2_0.ino` — main sketch (timer, simulation, state machine)
- `globals.h` — shared data structures and the `SyncMode` enum
- `wheel_defs.h` — trigger wheel definitions
- `comms.h`, `comms.cpp` — serial command parser (ArduStim protocol)
- `timing.h`, `timing.cpp` — injector/ignition input capture (Phase 2)
- `web.h`, `web.cpp` — WiFi AP + dashboard + `/setup` + `/api/data` (Phase 3)

> The previous `firmware/` folder (a historical duplicate with `*_working_*` backups) has been
> removed: there is now a single source of truth. The backups remain recoverable from git history.

## Main modules

- `timing.h`, `timing.cpp` — ISR-based input capture of the 9 Speeduino channels read through the
  CD4050BE buffers (4 injectors + 4 coils + fan). Measures injector on-time (ms), frequency, duty,
  ignition dwell and advance in degrees relative to the Crank edge (the ESP32 is the master).
  It also exposes the `hasRecentInjection/Ignition/Firing` helpers used by the Sync Mode startup logic.
- `web.h`, `web.cpp` — WiFi AP (`EngineSimulator2.0` / `1234567890` / `192.168.254.1`),
  web dashboard at `/`, setup page at `/setup` (trigger wheel + Sync Mode), JSON `/api/data`
  endpoint, persistence to NVS (namespace `engsim`, keys `wheel` and `syncMode`).

## Additional library dependencies

The firmware also requires:

- **ESP Async WebServer** (`ESP32Async` fork):
  `https://github.com/ESP32Async/ESPAsyncWebServer`
- **Async TCP** (`ESP32Async` fork, dependency of ESP Async WebServer):
  `https://github.com/ESP32Async/AsyncTCP`

> **IMPORTANT:** the original `me-no-dev` versions (classic `ESPAsyncWebServer` 3.1.0 and
> `AsyncTCP` 1.1.4) do NOT compile with ESP32 Core v3.3.x: 3.1.0 uses `mbedtls_md5_*_ret`
> (removed in mbedTLS 3) and 1.1.4 lacks the `const` qualifier on `AsyncServer::status()`.
> Use the `ESP32Async` forks (`ESP Async WebServer` + `Async TCP`) instead, which expose the
> same headers (`ESPAsyncWebServer.h`, `AsyncTCP.h`).

All other libraries ship with the ESP32 Arduino Core.

## Critical fix included (2026-09)

Inside the timer ISR `onCrankTimer()`, using `digitalWrite()` did NOT update the GPIO pins on
ESP32 Arduino Core v3.3.5 (Crank/Cam stayed at 0V even with the engine running, so Speeduino
never read the RPM).

**Applied fix:** replaced `digitalWrite` with direct GPIO register writes:
- HIGH: `GPIO.out_w1ts = (1ULL << pin);`
- LOW:  `GPIO.out_w1tc = (1ULL << pin);`

Added includes: `soc/gpio_struct.h` and `soc/gpio_reg.h`.

The same technique (direct `GPIO.in` access) is used inside the input-capture ISRs in
`timing.cpp` (do NOT use `digitalRead`/`digitalWrite` inside ISRs on core v3.3.5).

## Hardware validation

- Board: ESP32 DevKit1 (ESP32-D0WD-V3) on COM4
- Crank (GPIO12) / Cam1 (GPIO13) / Cam2 (GPIO14) wired at 3.3V
- Speeduino (Mega2560) reads the engine speed correctly
- Average voltage on Crank/Cam: ~1.6V (3.3V square wave, ~50% duty)
- Sync Mode: both modes (Passive and Active) validated from the `/setup` page

## Build command

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 "Engine_Simulator_2_0"

# first-time library install (EXACT names of the ESP32Async forks)
arduino-cli lib install "ESP Async WebServer" "Async TCP"

arduino-cli upload -p COMx --fqbn esp32:esp32:esp32 "Engine_Simulator_2_0"
```

> Note: arduino-cli / the Arduino IDE require the sketch to live in a folder named after the
> `.ino` file: hence the `Engine_Simulator_2_0/` folder.
