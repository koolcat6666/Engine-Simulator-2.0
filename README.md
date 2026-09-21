# Engine Simulator 2.0

Simulatore di motore per il test di ECU (**Speeduino V0.4**) basato su **ESP32-32U devKitC**.  
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

- **64 pattern ruote foniche** (compatibile ArduStim): 60-2, 36-1, 24-1, LS1, 58X, Chrysler NGC, BMW N20, GM 40-tooth OSS, e molti altri
- **Modello di inerzia RPM**: cranking 0->350 RPM, poi idle a 850 RPM + (TPS x 6150), con filtro passa-basso
- **Pulsante Start/Stop**: avvia il motore con rampa di cranking, stabilizzazione a idle, arresto graduale
- **Simulazione MAP dinamica**: 100 kPa a motore spento, 85 kPa in cranking, 30-100 kPa in funzione di RPM/TPS
- **Simulazione CLT dinamica**: riscaldamento graduale, raffreddamento con ventola, legge di Newton a motore spento
- **Interfaccia seriale USB**: protocollo ArduStim per configurazione ruota fonica e modalita' RPM da TunerStudio
- **Monitoraggio timing Speeduino (FASE 2)**: input capture via ISR sui 9 ingressi CD4050BE (4 iniettori + 4 candele + fan) — misura on-time iniettori (ms), frequenza, duty, dwell candele e anticipo in gradi BTDC
- **WiFi AP + Dashboard web (FASE 3)**: SSID `EngineSimulator2.0` / pass `1234567890` / IP `192.168.254.1`, dashboard fluida su `/`, selezione ruota fonica tra i 64 pattern e salvataggio su NVS su `/setup`, endpoint `/api/data` JSON

### Stato di verifica

| Modulo | Stato |
|:-------|:------|
| FASE 1 — Generazione Crank/Cam e modello motore | **Verificata su hardware** (Speeduino/Mega2560 legge i giri; fix registri `GPIO.out_w1ts/w1tc` in ISR per core v3.3.x) |
| FASE 2 — Timing iniettori/candele (input capture) | **Implementata e compilata, NON ancora verificata su hardware** |
| FASE 3 — WiFi AP + dashboard web | **Implementata e compilata, NON ancora verificata su hardware** |

Dettagli e riferimenti: `firmware/WORKING_VERSION.md`.

---

## Struttura delle Directory

```
Engine Simulator 2.0/
├── firmware/                    # Codice sorgente ESP32
│   ├── Engine_Simulator_2.0.ino # Sketch principale (timer, simulazione, state machine)
│   ├── comms.cpp                # Parser comandi seriale (protocollo ArduStim)
│   ├── comms.h                  # Header comms
│   ├── globals.h                # Strutture dati condivise
│   ├── timing.cpp               # Input capture timing iniettori/candele (FASE 2)
│   ├── timing.h                 # Header timing
│   ├── web.cpp                  # WiFi AP + dashboard + /setup + /api/data (FASE 3)
│   ├── web.h                    # Header web
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
| R-78E5.0-1.0 (RECOM) | 1 | Regolatore switching drop-in 12V -> 5V, 1A, 91% |
| Connettore IDC 40-pin | 1 | Connessione a Speeduino V0.4 |
| LED | 14 | Monitoraggio uscite ECU |
| Potenziometri trimmer | 3 | TPS, IAT, O2 (manuale) |
| Pulsante | 1 | Start/Stop |
| Jack DC | 1 | Alimentazione 12V |

### Livelli di Tensione

| Segmento | Tensione | Metodo |
|:---------|:--------:|:-------|
| Alimentazione board | 12V | Jack DC esterno |
| Alimentazione logica | 5V | R-78E5.0-1.0 (switching, drop-in) da 12V |
| ESP32 e sensori | 3.3V | Regolatore onboard ESP32 |
| Uscite digitali ESP32 -> ECU | 3.3V -> 5V | 74AHCT125 |
| Uscite analogiche ESP32 -> ECU | 0-3.3V -> 0-5V | TLV2372 (guadagno 1.5x) |
| Ingresso Fan da ECU | 5V -> 3.3V | Partitore resistivo |

---

## Lavorare su due PC (sync sessioni chat opencode)

Per continuare la stessa conversazione opencode su due PC (uno alla volta), le sessioni si esportano come JSON nel repo e si reimportano sull'altro PC.

### Prerequisito (una tantum, su entrambi i PC)

```powershell
winget install SST.opencode
```

### Trasferimento dal PC-A (quello su cui hai lavorato)

```powershell
.\export-chat.ps1               # scegli la sessione, verra' salvata in sessions/
git add sessions
git commit -m "chat: export sessione"
git push
```

### Ripresa sul PC-B

```powershell
git pull
.\import-chat.ps1 sessions\<file>.json
```

Poi apri opencode e riprendi la sessione con `/sessions` (o `opencode --continue`).

> Le sessioni esportate restano nel repo come JSON in `sessions/`. Non sincronizzare
> il database `~/.local/share/opencode/opencode.db` tra PC: e' un file binario che
> si corrompe facilmente se sincronizzato a mano.

---

## Build e Upload

### Prerequisiti

- [Arduino IDE](https://www.arduino.cc/en/software) o [Arduino CLI](https://arduino.github.io/arduino-cli/)
- Pacchetto board **esp32** installato (`esp32:esp32` via Board Manager)
- Scheda selezionata: `ESP32 Dev Module` o `ESP32-WROOM-32U`
- Librerie per la dashboard (FASE 3) — usare i fork **ESP32Async** (compatibili col
  core esp32 v3.3.x e mbedTLS 3; le versioni classiche `me-no-dev` non compilano):
  - **ESP Async WebServer** (`arduino-cli lib install "ESP Async WebServer"`)
  - **Async TCP** (`arduino-cli lib install "Async TCP"`)

### Compilazione e caricamento

```bash
# Via Arduino CLI (prima volta: installa anche le librerie della dashboard)
arduino-cli lib install "ESP Async WebServer" "Async TCP"
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/Engine_Simulator_2.0.ino
arduino-cli upload --fqbn esp32:esp32:esp32 --port COMx firmware/Engine_Simulator_2.0.ino
```

### Configurazione

La ruota fonica puo' essere selezionata tramite:
1. **Dashboard web** (consigliata): connettiti alla rete WiFi `EngineSimulator2.0` (password `1234567890`)
   e apri `http://192.168.254.1` -> pagina `/setup` per scegliere la ruota (salvataggio persistente su NVS)
2. **Seriale USB** (115200 baud): comando `X` per ruota successiva, `S<n>` per selezionare per indice
3. **ArduStim GUI**: software desktop che si collega alla seriale per configurare pattern e modalita'

---

## Note di Progetto

- Il **MAP sensor** integrato sulla Speeduino V0.4 (IDC Pin 11) non puo' essere usato come alternativa al DAC generato dall'ESP32 per le prove di simulazione. In questo caso, il pin MAP dell'IDC va collegato al PCB del simulatore.
- La **ventola** e' gestita in retroazione: Speeduino la controlla, l'ESP32 la legge per modellare la temperatura CLT.
- I **potenziometri IAT e O2** sono puramente manuali e non influenzano la simulazione dell'ESP32. Servono per testare le correzioni della ECU.
- Il **PCB KiCad** (`Hardware/Engine Simulator 2.0/`) e' completo: routing a 2 strati con piano GND su B.Cu in thermal relief. DRC: **0 errori di clearance e 0 piazzole sconnesse**; restano solo warning cosmetici (altezza testo serigrafia e librerie footprint locali non abilitate). Il report aggiornato e' in `Engine Simulator 2.0-drc.rpt`.

### Output di fabbricazione (Gerber + Drill)

I file di fabbricazione sono generati con `kicad-cli` e pronti per qualsiasi fab (JLCPCB/PCBWay/PCBcart):

- **`Hardware/Engine Simulator 2.0/Gerbers/`** — Gerber RS-274X a 9 layer (`F_Cu`, `B_Cu`, `F_Paste`, `B_Paste`, `F_Mask`, `B_Mask`, `F_SilkS`, `B_SilkS`, `Edge_Cuts`) + job file `.gbrjob`.
- **`Hardware/Engine Simulator 2.0/Gerbers/`** — Drill Excellon separati: `-PTH.drl` (fori placcati, 0,3–2,2 mm) e `-NPTH.drl` (0 NPTH).
- **`Hardware/Engine Simulator 2.0/Engine_Simulator_2.0_FAB_gerbers.zip`** — archivio zip di tutti i Gerber+drill pronto per l'upload diretto sul sito del fab.

Scheda: **2 strati, 100×100 mm, spessore 1,6 mm, HASL**, pista min. 0,2 mm, clearance min. 0,21 mm, foro drill min. 0,3 mm (via) / 2,2 mm (fori montaggio M2, PTH collegato a GND).
