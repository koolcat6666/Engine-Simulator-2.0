# Speeduino V0.4 - IDC 40 Pin Connector Pinout

Pinout ufficiale del connettore IDC 40 pin della **Speeduino V0.4**.  
Riferimento: [Speeduino Wiki - V04 Board](https://wiki.speeduino.com/en/boards/V04)

## Tabelle Pinout IDC 40 (Speeduino V0.4)

| Pin IDC | Funzione |
|:-------:|:---------|
| 1 | Injector 1 - Pin 1/2 |
| 2 | Injector 2 - Pin 1/2 |
| 3 | Injector 3 - Pin 1/2 |
| 4 | Injector 3 - Pin 2/2 |
| 5 | Injector 4 - Pin 1/2 |
| 6 | Injector 4 - Pin 2/2 |
| 7 | Ignition 1 |
| 8 | Ignition 4 |
| 9 | Ground (Sensori) |
| 10 | Ground |
| 11 | MAP Sensor (0-5V) |
| 12 | Ground |
| 13 | +5V |
| 14 | Proto Area 1 (v0.4.4b+: Flex Sensor) |
| 15 | Proto Area 2 (v0.4.4b+: Fan) |
| 16 | Proto Area 3 (v0.4.4b+: Fuel Pump) |
| 17 | Proto Area 4 (v0.4.4b+: Tachometer) |
| 18 | Proto Area 5 (v0.4.4b+: Clutch) |
| 19 | CLT (Coolant Temperature) |
| 20 | IAT (Inlet Air Temperature) |
| 21 | O2 Sensor |
| 22 | TPS (Throttle Position Sensor) |
| 23 | Ground |
| 24 | Cam Input / VR2+ |
| 25 | Crank Input / VR1+ |
| 26 | VR2- (non usato per sensori Hall) |
| 27 | VR1- (non usato per sensori Hall) |
| 28 | +5V |
| 29 | Stepper Motor 2B |
| 30 | Stepper Motor 2A |
| 31 | Stepper Motor 1A |
| 32 | Stepper Motor 1B |
| 33 | Ignition 3 |
| 34 | Ignition 2 |
| 35 | Boost Control |
| 36 | Idle 2 (3-wire idle valve) |
| 37 | PWM Idle (1-wire idle) |
| 38 | VVT |
| 39 | Injector 2 - Pin 2/2 |
| 40 | Injector 1 - Pin 2/2 |

---

## Funzioni Default delle Uscite (v0.4.4b+)

| Funzione | Pin IDC | Arduino Pin | Note |
|:---------|:-------:|:-----------:|:-----|
| Boost Control | 35 | 7 | Disabilitare in TunerStudio e rimappare a Pin 4 |
| VVT | 38 | 4 | Disabilitare in TunerStudio |
| Idle 1 (PWM) | 37 | 5 | |
| Idle 2 (3-wire) | 36 | 6 | |
| Fuel Pump | 16 | 45 | Jumper da proto area |
| Fan | 15 | 47 | Jumper da proto area |
| Tachometer | 17 | 49 | Jumper da proto area |
| Launch/Clutch | 18 | 51 | Jumper da proto area |

---

## Sensori di Input (Pezzo 0.4)

| Funzione | Arduino Pin | Descrizione |
|:---------|:-----------:|:------------|
| Crank Trigger | 19 | Sensore CKP (VR o Hall) |
| Cam Trigger | 18 | Sensore CMP (VR o Hall) |
| Cam 2 / VVT2 | 3 | Secondo sensore fase |
| TPS | A2 | Throttle Position Sensor |
| MAP | A3 | MAP Sensor (integrato sulla board) |
| IAT | A0 | Intake Air Temperature |
| CLT | A1 | Coolant Temperature |
| O2 | A8 | Sonda Lambda |
| Battery | A4 | Riferimento tensione batteria |

---

## Collegamenti verso Engine Simulator 2.0 (ESP32)

Di seguito la mappatura dei pin IDC collegati all'ESP32 sul PCB del simulatore:

| Pin IDC | Funzione Speeduino | Direzione (per ECU) | Collegamento ESP32 | Note Hardware |
|:-------:|:------------------:|:-------------------:|:------------------:|:--------------|
| 11 | MAP | **Ingresso ECU** | DAC1 (GPIO 25) via op-amp | Generato da ESP32 (0-3.3V amplificato a 0-5V) |
| 19 | CLT | **Ingresso ECU** | DAC2 (GPIO 26) via op-amp | Generato da ESP32 (0-3.3V amplificato a 0-5V) |
| 20 | IAT | **Ingresso ECU** | Potenziometro manuale | Regolatore esterno, collegato direttamente a Speeduino |
| 22 | TPS | **Ingresso ECU** | Potenziometro con partitore | Regolatore esterno, collegato a Speeduino + partitore verso ESP32 ADC (GPIO 34) |
| 21 | O2 | **Ingresso ECU** | Potenziometro manuale | Regolatore esterno, collegato direttamente a Speeduino |
| 25 | Crank | **Uscita ECU** (input per Speeduino) | GPIO 12 via level shifter 3.3V->5V | Segnale generato da ESP32 |
| 24 | Cam | **Uscita ECU** (input per Speeduino) | GPIO 13 via level shifter 3.3V->5V | Segnale generato da ESP32 |
| 15 | Fan | **Uscita ECU** | CD4050-A out 6 -> GPIO 27 | Lettura stato ventola (temperatura CLT) |
| 1 | Injector 1 | **Uscita ECU** | CD4050-B out 4 -> GPIO 15 | Input capture timing (FASE 2) |
| 2 | Injector 2 | **Uscita ECU** | CD4050-B out 6 -> GPIO 16 | Input capture timing (FASE 2) |
| 3 | Injector 3 | **Uscita ECU** | CD4050-B out 10 -> GPIO 17 | Input capture timing (FASE 2) |
| 5 | Injector 4 | **Uscita ECU** | CD4050-B out 12 -> GPIO 18 | Input capture timing (FASE 2) |
| 7 | Ignition 1 | **Uscita ECU** | CD4050-A out 10 -> GPIO 19 | Input capture timing (FASE 2) |
| 8 | Ignition 4 | **Uscita ECU** | CD4050-A out 2 -> GPIO 23 | Input capture timing (FASE 2) |
| 33 | Ignition 3 | **Uscita ECU** | CD4050-A out 15 -> GPIO 22 | Input capture timing (FASE 2) |
| 34 | Ignition 2 | **Uscita ECU** | CD4050-A out 12 -> GPIO 21 | Input capture timing (FASE 2) |
| 37 | Idle (PWM) | **Uscita ECU** | Non monitorata | (Monitorabile in futuro) |
| 35 | Boost | **Uscita ECU** | Non monitorata | (Monitorabile in futuro) |
| 38 | VVT | **Uscita ECU** | Non monitorata | (Monitorabile in futuro) |
| 16 | Fuel Pump | **Uscita ECU** | Non monitorata | (Monitorabile in futuro) |

---

## Note sui Livelli di Tensione

### Uscite ESP32 -> Speeduino (3.3V -> 5V)
I segnali digitali generati dall'ESP32 (Crank, Cam1) sono a logica 3.3V. Devono essere traslati a 5V tramite **74AHCT125** (level shifter) prima di essere inviati ai pin Crank/Cam dell'IDC.

### Uscite Analogiche ESP32 -> Speeduino
I DAC dell'ESP32 (GPIO 25, 26) erogano 0-3.3V. Vanno amplificati a 0-5V tramite **op-amp rail-to-rail** (TLV2372) con guadagno ~1.5x per compatibilita' con gli ingressi analogici di Speeduino (MAP e CLT).

### Ingressi da Speeduino -> ESP32 (5V -> 3.3V)
Il segnale Fan (pin 15 IDC) e' a 5V. Deve essere ridotto a 3.3V tramite partitore resistivo prima di essere letto dal GPIO 27 dell'ESP32.

### Potenziometri TPS/IAT/O2
I potenziometri sono alimentati a 5V (condiviso con Speeduino). Il cursore va collegato:
- **TPS**: a Speeduino (pin 22 IDC) + partitore resistivo 10k/20k verso GPIO 34 ESP32
- **IAT**: direttamente a Speeduino (pin 20 IDC), nessun collegamento ESP32
- **O2**: direttamente a Speeduino (pin 21 IDC), nessun collegamento ESP32
