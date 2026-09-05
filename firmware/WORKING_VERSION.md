# Versione Funzionante del Firmware

**Stato (2026-09-04):** questa versione e' confermata FUNZIONANTE su hardware.

## File di riferimento (versione funzionante)

- `Engine_Simulator_2.0.ino`
- `globals.h`
- `wheel_defs.h`
- `comms.h`, `comms.cpp`

Backup esplicito della versione funzionante:
- `Engine_Simulator_2.0_working_20260904.ino`
- `globals_working_20260904.h`
- `wheel_defs_working_20260904.h`

## Corriszione critica inclusa (2026-09)

Sulla ISR del timer `onCrankTimer()` l'uso di `digitalWrite()` NON aggiornava i pin
GPIO su ESP32 Arduino Core v3.3.5 (Crank/Cam restavano a 0V anche a motore avviato,
quindi Speeduino non leggeva i giri).

**Fix applicata:** sostituito `digitalWrite` con la scrittura diretta ai registri GPIO:
- HIGH: `GPIO.out_w1ts = (1ULL << pin);`
- LOW:  `GPIO.out_w1tc = (1ULL << pin);`

Include aggiunti: `soc/gpio_struct.h` e `soc/gpio_reg.h`.

## Verifica hardware

- Scheda: ESP32 DevKit1 (ESP32-D0WD-V3) su COM4
- Collegamento Crank (GPIO12) / Cam1 (GPIO13) / Cam2 (GPIO14) a 3.3V
- Speeduino (Mega2560) legge correttamente i giri
- Tensione media su Crank/Cam: ~1.6V (square 3.3V, duty ~50%)

## Comando di compilazione

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/Engine_Simulator_2.0.ino
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32 firmware/Engine_Simulator_2.0.ino
```

> Nota: arduino-cli richiede che lo sketch stia in una cartella con lo stesso nome del file
> `.ino`. La copia pronta per Arduino IDE e' in `../Engine_Simulator_2_0/` (allineata).
