# ESP32U Pin Assignment - Engine Simulator 2.0

Pin assignment for the **ESP32-WROOM-32U** per il simulatore di motore.

L'ESP32U si distingue per la presenza del connettore U.FL per l'antenna esterna, ma mantiene la stessa piedinatura e pinout standard dei moduli ESP32 WROOM a 38 pin.

---

## Signal Architecture

L'ESP32 si occupa esclusivamente di:
- **Generare** i segnali di ruota fonica (Crank, Cam1, Cam2)
- **Generare** le tensioni analogiche per MAP e CLT verso Speeduino
- **Leggere** il TPS (per controllare gli RPM) e lo stato della ventola
- **Gestire** il pulsante Start/Stop

I segnali IAT e O2 sono gestiti **manualmente** tramite potenziometri collegati direttamente a Speeduino, senza intervento dell'ESP32.

---

## ESP32U Pin Connection Map

### Digital Inputs & Buttons
*   **GPIO 4** (Pin 26): **Pulsante Start/Stop**
    *   *Descrizione:* Ingresso digitale per avviare/arrestare il motore.
    *   *Nota:* Configurato con pull-up interno. Il pulsante deve chiudere verso GND.

*   **GPIO 27** (Pin 12): **Input Stato Ventola (ECU Fan Feedback)**
    *   *Descrizione:* Rileva se Speeduino ha attivato la ventola di raffreddamento.
    *   *Nota:* Speeduino lavora a 5V. **Richiede partitore resistivo o level shifter (5V -> 3.3V)** prima di arrivare a questo pin. Attivo a livello logico basso (GND = Ventola accesa).
    *   *Collegamento Speeduino:* IDC Pin 15 (Proto Area 2 / Fan)

---

### Analog Inputs (Sensori)
*   **GPIO 34** (Pin 6 - ADC1_CH6): **Ingresso Potenziometro TPS (Acceleratore)**
    *   *Descrizione:* Legge il valore del reostato dell'acceleratore per determinare il target RPM.
    *   *Nota:* Il potenziometro TPS del simulatore e' alimentato a 5V (condiviso con Speeduino). Il cursore centrale va collegato a questo pin dell'ESP32 **esclusivamente tramite partitore resistivo** (es. 10k e 20k ohm) per limitare la tensione massima a 3.3V. Lo stesso segnale va anche al pin TPS di Speeduino (IDC Pin 22).
    *   *Collegamento Speeduino:* IDC Pin 22 (TPS) - parte comune del potenziometro

*   **GPIO 35 e GPIO 32**: **NOT USED** dall'ESP32. IAT e O2 sono gestiti da potenziometri collegati direttamente a Speeduino.

---

### Analog Outputs (Simulazione Sensori per ECU)
*   **GPIO 25** (Pin 10 - DAC1): **Output Analogico MAP (Pressione)**
    *   *Descrizione:* Uscita analogica (DAC 8-bit) che genera la tensione di pressione del collettore in base alla dinamica del motore.
    *   *Nota:* Genera un segnale 0-3.3V. Per interfacciarsi con i 5V di Speeduino, viene utilizzato un amplificatore operazionale TLV2372 con guadagno ~1.5x per coprire il range 0-5V.
    *   *Collegamento Speeduino:* IDC Pin 11 (MAP Sensor)

*   **GPIO 26** (Pin 11 - DAC2): **Output Analogico CLT (Temperatura Motore)**
    *   *Descrizione:* Uscita analogica (DAC 8-bit) che genera la tensione della temperatura dell'acqua simulando la curva termica.
    *   *Nota:* Genera un segnale 0-3.3V. Amplificato a 0-5V tramite op-amp per Speeduino.
    *   *Collegamento Speeduino:* IDC Pin 19 (CLT)

---

### Digital Outputs (Generazione Ruote Foniche)
*   **GPIO 12** (Pin 14): **Crank Trigger Output** (Giri Motore)
    *   *Descrizione:* Genera l'onda quadra per il sensore di giri (CKP).
    *   *Nota:* **Richiede level shifter digitale 3.3V -> 5V** (74AHCT125) prima di andare al pin Crank di Speeduino.
    *   *Collegamento Speeduino:* IDC Pin 25 (Crank Input / VR1+)

*   **GPIO 13** (Pin 16): **Cam 1 Trigger Output** (Fase Motore)
    *   *Descrizione:* Genera il segnale per il sensore di fase 1 (CMP1).
    *   *Nota:* **Richiede level shifter digitale 3.3V -> 5V** (74AHCT125).
    *   *Collegamento Speeduino:* IDC Pin 24 (Cam Input / VR2+)

*   **GPIO 14** (Pin 13): **Cam 2 Trigger Output** (Fase Motore Secondaria)
    *   *Descrizione:* Genera il segnale di fase secondario (CMP2) per ruote foniche complesse.
    *   *Nota:* **Richiede level shifter digitale 3.3V -> 5V** (74AHCT125). Non sempre necessario.

---

## Riassunto Tabellare Pinout ESP32U

| Nome Pin ESP32U | Tipo | Direzione | Funzione nel Simulatore | Collegamento ECU / PCB |
|:---------------:|:----:|:---------:|:-----------------------:|:------------------------|
| **GPIO 4**      | D    | Input     | Pulsante Start/Stop     | Pulsante fisico a GND |
| **GPIO 12**     | D    | Output    | Crank Trigger           | Level Shifter -> IDC Pin 25 |
| **GPIO 13**     | D    | Output    | Cam 1 Trigger           | Level Shifter -> IDC Pin 24 |
| **GPIO 14**     | D    | Output    | Cam 2 Trigger           | Level Shifter (opzionale) |
| **GPIO 25**     | A    | Output    | MAP Sensor (DAC1)       | Op-amp (guadagno 1.5x) -> IDC Pin 11 |
| **GPIO 26**     | A    | Output    | CLT Sensor (DAC2)       | Op-amp -> IDC Pin 19 |
| **GPIO 27**     | D    | Input     | Feedback Ventola (FAN)  | Partitore 5V->3.3V <- IDC Pin 15 |
| **GPIO 34**     | A    | Input     | Monitor TPS             | Partitore <- Potenziometro TPS |
| **GND**         | P    | -         | Massa Comune            | GND comune Speeduino/ESP32 |

---

## Pin NON Utilizzati dall'ESP32

I seguenti segnali sono gestiti **senza intervento dell'ESP32** (potenziometri manuali collegati direttamente a Speeduino):

| Segnale | Pin IDC Speeduino | Note |
|:--------|:-----------------:|:-----|
| IAT (Temperatura Aria) | IDC Pin 20 | Potenziometro esterno, direttamente a Speeduino |
| O2 (Sonda Lambda) | IDC Pin 21 | Potenziometro esterno, direttamente a Speeduino |
