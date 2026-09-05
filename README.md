# Engine Simulator 2.0

Simulatore di motore per il test di ECU (**Speeduino V0.4**) basato su **ESP32-WROOM-32U**.  
Il sistema si collega alla centralina tramite il connettore IDC 40-pin standard di Speeduino e genera i segnali necessari per simulare il funzionamento di un motore reale.

## Architettura

### Segnali generati dall'ESP32

| Segnale | Pin ESP32 | Verso Speeduino | Metodo |
|:--------|:---------:|:---------------:|:-------|
| Crank Trigger | GPIO 12 | IDC Pin 25 | Onda quadra via timer HW, level shifter 3.3V->5V |
| Cam 1 Trigger | GPIO 13 | IDC Pin 24 | Onda quadra via timer HW, level shifter 3.3V->5V |
| Cam 2 Trigger | GPIO 14 | IDC Pin 24 (opz.) | Onda quadra via timer HW, level shifter 3.3V->5V |
| CLT (Temperatura) | GPIO 26 (DAC2) | IDC Pin 19 | Tensione analogica 0-3.3V, amplificata a 0-5V |
| MAP (Pressione) | GPIO 25 (DAC1) | IDC Pin 11 | Tensione analogica 0-3.3V, amplificata a 0-5V |

### Segnali gestiti manualmente (potenziometri)

| Segnale | Pin Speeduino | Note |
|:--------|:-------------:|:-----|
| TPS (Acceleratore) | IDC Pin 22 | Potenziometro a 5V, parte comune verso ESP32 ADC (GPIO 34) via partitore |
| IAT (Temperatura Aria) | IDC Pin 20 | Potenziometro collegato direttamente a Speeduino |
| O2 (Sonda Lambda) | IDC Pin 21 | Potenziometro collegato direttamente a Speeduino |

### Segnali in ingresso dall'ECU

| Segnale | Pin ESP32 | Da Speeduino | Note |
|:--------|:---------:|:------------:|:-----|
| Fan Feedback | GPIO 27 | IDC Pin 15 | Stato ventola (0V = accesa), partitore 5V->3.3V |

### LED di Monitoraggio Uscite ECU

| LED | Uscita Speeduino | Funzione |
|:----|:----------------:|:---------|
| D1-D4 | IDC Pin 1,2,3,5 | Iniettori 1-4 |
| D5-D8 | IDC Pin 7,8,33,34 | Candele (Ignition 1-4) |
| D9 | IDC Pin 15 | Ventola raffreddamento |
| D10 | IDC Pin 37 | Valvola del minimo (Idle) |
| D11 | IDC Pin 38 | VVT |
| D12 | IDC Pin 35 | Boost |
| D13 | IDC Pin 16 | Pompa carburante |

---

## Funzionalita' Firmware

- **63 pattern ruote foniche** (compatibile ArduStim): 60-2, 36-1, 24-1, LS1, 58X, Chrysler NGC, BMW N20, e molti altri
- **Modello di inerzia RPM**: cranking 0->350 RPM, poi idle a 850 RPM + (TPS x 6150), con filtro passa-basso
- **Pulsante Start/Stop**: avvia il motore con rampa di cranking, stabilizzazione a idle, arresto graduale
- **Simulazione MAP dinamica**: 100 kPa a motore spento, 85 kPa in cranking, 30-100 kPa in funzione di RPM/TPS
- **Simulazione CLT dinamica**: riscaldamento graduale, raffreddamento con ventola, legge di Newton a motore spento
- **Interfaccia seriale USB**: protocollo ArduStim per configurazione ruota fonica e modalita' RPM da TunerStudio

---

## Struttura delle Directory

```
Engine Simulator 2.0/
├── firmware/                    # Codice sorgente ESP32
│   ├── Engine_Simulator_2.0.ino # Sketch principale (timer, simulazione, state machine)
│   ├── comms.cpp                # Parser comandi seriale (protocollo ArduStim)
│   ├── comms.h                  # Header comms
│   ├── globals.h                # Strutture dati condivise
│   └── wheel_defs.h             # 63 pattern ruote foniche
├── Engine_Simulator_2_0/        # Copia Arduino IDE (cartella = nome .ino)
├── Hardware/                    # Design PCB (KiCad 10.0)
│   └── Engine Simulator 2.0/
│       ├── *.kicad_sch          # Schema elettrico
│       ├── *.kicad_pcb          # Layout PCB
│       └── *.kicad_pro          # Progetto KiCad
├── doc/                         # Documentazione
│   ├── dupont_pinout.md         # Pinout IDC 40-pin Speeduino V0.4
│   └── esp32u_pin_assignment.md # Mappa pin ESP32U
└── README.md                    # Questo file
```

---

## Requisiti Hardware

### Componenti Principali

| Componente | Quantita' | Descrizione |
|:-----------|:---------:|:------------|
| ESP32-WROOM-32U | 1 | Microcontroller principale |
| 74AHCT125 | 4 | Level shifter 3.3V -> 5V (Crank, Cam1, Cam2) |
| TLV2372 | 3 | Op-amp rail-to-rail (buffer MAP, CLT DAC) |
| L7805 | 1 | Regolatore lineare 12V -> 5V |
| Connettore IDC 40-pin | 1 | Connessione a Speeduino V0.4 |
| LED | 14 | Monitoraggio uscite ECU |
| Potenziometri trimmer | 3 | TPS, IAT, O2 (manuale) |
| Pulsante | 1 | Start/Stop |
| Jack DC | 1 | Alimentazione 12V |

### Livelli di Tensione

| Segmento | Tensione | Metodo |
|:---------|:--------:|:-------|
| Alimentazione board | 12V | Jack DC esterno |
| Alimentazione logica | 5V | L7805 da 12V |
| ESP32 e sensori | 3.3V | Regolatore onboard ESP32 |
| Uscite digitali ESP32 -> ECU | 3.3V -> 5V | 74AHCT125 |
| Uscite analogiche ESP32 -> ECU | 0-3.3V -> 0-5V | TLV2372 (guadagno 1.5x) |
| Ingresso Fan da ECU | 5V -> 3.3V | Partitore resistivo |

---

## Build e Upload

### Prerequisiti

- [Arduino IDE](https://www.arduino.cc/en/software) o [Arduino CLI](https://arduino.github.io/arduino-cli/)
- Pacchetto board **esp32** installato (`esp32:esp32` via Board Manager)
- Scheda selezionata: `ESP32 Dev Module` o `ESP32-WROOM-32U`

### Compilazione e caricamento

```bash
# Via Arduino CLI
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/Engine_Simulator_2.0.ino
arduino-cli upload --fqbn esp32:esp32:esp32 --port COMx firmware/Engine_Simulator_2.0.ino
```

### Configurazione

La ruota fonica puo' essere selezionata tramite:
1. **Seriale USB** (115200 baud): comando `X` per ruota successiva, `S<n>` per selezionare per indice
2. **ArduStim GUI**: software desktop che si collega alla seriale per configurare pattern e modalita'

---

## Note di Progetto

- Il **MAP sensor** integrato sulla Speeduino V0.4 (IDC Pin 11) puo' essere usato come alternativa al DAC generato dall'ESP32. In questo caso, il pin MAP dell'IDC non va collegato al PCB del simulatore.
- La **ventola** e' gestita in retroazione: Speeduino la controlla, l'ESP32 la legge per modellare la temperatura CLT.
- I **potenziometri IAT e O2** sono puramente manuali e non influenzano la simulazione dell'ESP32. Servono per testare le correzioni della ECU.

### Corrisotti noti nel firmware (2026-09)

- **Uscite Crank/Cam non funzionanti:** sulla ISR del timer `onCrankTimer()` l'uso di `digitalWrite()` non aggiornava i pin GPIO su ESP32 Arduino Core v3.3.5 (Crank/Cam rimanevano a 0V anche a motore avviato, quindi Speeduino non leggeva i giri). **Fix:** sostituito `digitalWrite` con la scrittura diretta ai registri GPIO (`GPIO.out_w1ts` / `GPIO.out_w1tc`), che funziona in ISR. Dopo la patch Speeduino (Mega2560) legge correttamente i giri.
