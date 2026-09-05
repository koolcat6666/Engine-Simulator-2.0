# Versione Funzionante del Firmware

**Stato (2026-09-04):** questa versione e' confermata FUNZIONANTE su hardware (FASE 1).

**Stato (branch feature/fase2-timing-dashboard):** FASE 2 (timing iniettori/candele) e
FASE 3 (WiFi AP + dashboard web) in implementazione — NON ancora verificate su hardware.

## File di riferimento (versione funzionante)

- `Engine_Simulator_2.0.ino`
- `globals.h`
- `wheel_defs.h`
- `comms.h`, `comms.cpp`

Backup esplicito della versione funzionante:
- `Engine_Simulator_2.0_working_20260904.ino`
- `globals_working_20260904.h`
- `wheel_defs_working_20260904.h`

## Nuovi moduli (branch feature/fase2-timing-dashboard)

- `timing.h`, `timing.cpp` — input capture su ISR dei 9 canali Speeduino letti via
  CD4050BE (4 iniettori + 4 candele + fan). Misura on-time iniettore (ms), frequenza,
  duty, dwell candela e anticipo in gradi rispetto al fronte Crank (l'ESP32 e' il master).
- `web.h`, `web.cpp` — WiFi AP (`EngineSimulator2.0` / `1234567890` / `192.168.254.1`),
  dashboard web su `/`, setup ruota fonica su `/setup`, endpoint `/api/data` JSON,
  salvataggio `config.wheel` su NVS (namespace `engsim`, key `wheel`).

## Dipendenze librerie aggiuntive

Il firmware ora richiede anche:
- **ESP Async WebServer** (fork `ESP32Async`, qui usato v3.9.4):
  `https://github.com/ESP32Async/ESPAsyncWebServer`
- **Async TCP** (fork `ESP32Async`, dipendenza di ESP Async WebServer, qui v3.5.0):
  `https://github.com/ESP32Async/AsyncTCP`

> **IMPORTANTE**: le versioni originali `me-no-dev` (`ESPAsyncWebServer` 3.1.0 e
> `AsyncTCP` 1.1.4, installate con il nome classico) NON compilano con ESP32 Core v3.3.11:
> la 3.1.0 usa `mbedtls_md5_*_ret` (rimosse da mbedTLS 3) e la 1.1.4 manca del qualifier
> `const` su `AsyncServer::status()`. Vanno quindi usati i fork `ESP32Async`
> (`ESP Async WebServer` + `Async TCP`), che espongono gli stessi header
> (`ESPAsyncWebServer.h`, `AsyncTCP.h`).

Tutte le altre librerie sono di base dell'ESP32 Arduino Core.

## Corriszione critica inclusa (2026-09)

Sulla ISR del timer `onCrankTimer()` l'uso di `digitalWrite()` NON aggiornava i pin
GPIO su ESP32 Arduino Core v3.3.5 (Crank/Cam restavano a 0V anche a motore avviato,
quindi Speeduino non leggeva i giri).

**Fix applicata:** sostituito `digitalWrite` con la scrittura diretta ai registri GPIO:
- HIGH: `GPIO.out_w1ts = (1ULL << pin);`
- LOW:  `GPIO.out_w1tc = (1ULL << pin);`

Include aggiunti: `soc/gpio_struct.h` e `soc/gpio_reg.h`.

La stessa tecnica (accesso diretto a `GPIO.in`) e' usata nelle ISR di input capture
di `timing.cpp` (NON usare `digitalRead`/`digitalWrite` dentro le ISR su core v3.3.5).

## Verifica hardware

- Scheda: ESP32 DevKit1 (ESP32-D0WD-V3) su COM4
- Collegamento Crank (GPIO12) / Cam1 (GPIO13) / Cam2 (GPIO14) a 3.3V
- Speeduino (Mega2560) legge correttamente i giri
- Tensione media su Crank/Cam: ~1.6V (square 3.3V, duty ~50%)

## Comando di compilazione

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/Engine_Simulator_2.0.ino

# prima installazione delle librerie (NOMI ESATTI dei fork ESP32Async)
arduino-cli lib install "ESP Async WebServer" "Async TCP"

arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32 firmware/Engine_Simulator_2.0.ino
```

> Nota: arduino-cli richiede che lo sketch stia in una cartella con lo stesso nome del file
> `.ino`. La copia pronta per Arduino IDE e' in `../Engine_Simulator_2_0/` (allineata al
> codice nella branch).
