# Versione Funzionante del Firmware

**Stato (2026-09-04):** FASE 1 (generazione Crank/Cam + modello motore) confermata FUNZIONANTE su hardware.

**Stato (FASE 2 + FASE 3):** timing iniettori/candele (FASE 2), WiFi AP + dashboard web (FASE 3)
e la modalita' di sincronizzazione all'avvio (Sync Mode Passive/Active) sono implementate,
compilate e verificate su hardware.

## Sorgente unica

Il firmware e' pubblicato in una sola cartella: **`Engine_Simulator_2_0/`** (il nome della
cartella coincide con quello dello sketch `.ino`, requisito di Arduino IDE / arduino-cli).

File principali:

- `Engine_Simulator_2_0.ino` — sketch principale (timer, simulazione, state machine)
- `globals.h` — strutture dati condivise e enum `SyncMode`
- `wheel_defs.h` — definizioni delle ruote foniche
- `comms.h`, `comms.cpp` — parser seriale (protocollo ArduStim)
- `timing.h`, `timing.cpp` — input capture iniettori/candele (FASE 2)
- `web.h`, `web.cpp` — WiFi AP + dashboard + `/setup` + `/api/data` (FASE 3)

> La vecchia cartella `firmware/` (duplicato storico con backup `*_working_*`) e' stata rimossa:
> resta un'unica origine di verita'. I backup sono recuperabili dalla cronologia git.

## Moduli principali

- `timing.h`, `timing.cpp` — input capture su ISR dei 9 canali Speeduino letti via
  CD4050BE (4 iniettori + 4 candele + fan). Misura on-time iniettore (ms), frequenza,
  duty, dwell candela e anticipo in gradi rispetto al fronte Crank (l'ESP32 e' il master).
  Espone inoltre gli helper `hasRecentInjection/Ignition/Firing` usati dalla logica di
  avvio Sync Mode.
- `web.h`, `web.cpp` — WiFi AP (`EngineSimulator2.0` / `1234567890` / `192.168.254.1`),
  dashboard web su `/`, setup su `/setup` (ruota fonica + Sync Mode), endpoint `/api/data`
  JSON, salvataggio su NVS (namespace `engsim`, key `wheel` e `syncMode`).

## Dipendenze librerie aggiuntive

Il firmware richiede anche:
- **ESP Async WebServer** (fork `ESP32Async`):
  `https://github.com/ESP32Async/ESPAsyncWebServer`
- **Async TCP** (fork `ESP32Async`, dipendenza di ESP Async WebServer):
  `https://github.com/ESP32Async/AsyncTCP`

> **IMPORTANTE**: le versioni originali `me-no-dev` (`ESPAsyncWebServer` 3.1.0 e
> `AsyncTCP` 1.1.4, installate con il nome classico) NON compilano con ESP32 Core v3.3.x:
> la 3.1.0 usa `mbedtls_md5_*_ret` (rimosse da mbedTLS 3) e la 1.1.4 manca del qualifier
> `const` su `AsyncServer::status()`. Vanno quindi usati i fork `ESP32Async`
> (`ESP Async WebServer` + `Async TCP`), che espongono gli stessi header
> (`ESPAsyncWebServer.h`, `AsyncTCP.h`).

Tutte le altre librerie sono di base dell'ESP32 Arduino Core.

## Correzione critica inclusa (2026-09)

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
- Sync Mode: verificate entrambe le modalita' (Passive e Active) dalla pagina `/setup`

## Comando di compilazione

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 "Engine_Simulator_2_0"

# prima installazione delle librerie (NOMI ESATTI dei fork ESP32Async)
arduino-cli lib install "ESP Async WebServer" "Async TCP"

arduino-cli upload -p COMx --fqbn esp32:esp32:esp32 "Engine_Simulator_2_0"
```

> Nota: arduino-cli / Arduino IDE richiedono che lo sketch stia in una cartella con lo
> stesso nome del file `.ino`: da qui la cartella `Engine_Simulator_2_0/`.
