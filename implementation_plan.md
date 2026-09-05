# Engine Simulator 2.0 - Implementation Plan

Il progetto **Engine Simulator 2.0** ha lo scopo di creare un simulatore ECU basato su ESP32, compatibile a livello seriale con ArduStim, ma arricchito con la simulazione fisica dei parametri motore (MAP, CLT, inerzia RPM basata su TPS, sequenza di cranking con pulsante di Start/Stop) e un'interfaccia hardware (PCB) per connettersi direttamente a Speeduino tramite un connettore Dupont a 40 pin.

---

## User Review Required

> [!IMPORTANT]
> **Compatibilità di Tensione e Level Shifting:**
> - ESP32 lavora a **3.3V** sia per gli ingressi analogici (ADC max 3.3V) che per i GPIO.
> - Speeduino lavora a **5V**.
> - I potenziometri (TPS, IAT, CLT, O2) saranno alimentati a 5V per Speeduino. Per far leggere il TPS anche a ESP32 senza bruciare l'ADC, si utilizzerà un partitore resistivo o un buffer sul PCB.
> - Le uscite dell'ESP32 (Crank, Cam, Cam2) devono passare attraverso un level shifter a 5V prima di raggiungere Speeduino.
> - Gli ingressi di Speeduino letti da ESP32 (es. comando Ventola di raffreddamento a 5V) passeranno attraverso un level shifter a 3.3V.

> [!TIP]
> **Generazione Analogica (CLT e MAP):**
> L'ESP32 ha 2 canali DAC integrati a 8 bit (GPIO25 e GPIO26). Li useremo per generare le tensioni analogiche di MAP e CLT (che andranno poi traslate a 0-5V tramite op-amp sul PCB se Speeduino richiede range 0-5V, dato che i DAC di ESP32 erogano 0-3.3V).

---

## Proposed Changes

Creeremo la cartella `C:\Users\nicola\Desktop\Engine Simulator 2.0` con la seguente struttura di file:

### Componente 1: Firmware ESP32
Adatteremo la logica di generazione delle ruote foniche di ArduStim per usare i timer hardware di ESP32 (tramite le API ESP32 Arduino Core `esp32-hal-timer.h`), implementando al contempo le routine di simulazione fisica in background.

#### [NEW] [Engine_Simulator_2.0.ino](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/firmware/Engine_Simulator_2.0.ino)
Contiene il punto di ingresso dell'applicazione, la configurazione dei timer hardware di ESP32 per la ruota fonica e il loop principale che gestisce:
- Lettura pulsante Start/Stop (con debouncing e macchina a stati del motore: `SPENTO`, `CRAKING`, `ACCESO`, `SPEGNIMENTO`).
- Lettura TPS (ADC ESP32) e calcolo dinamico degli RPM correnti applicando le leggi di inerzia (accelerazione e decelerazione simulate).
- Calcolo e aggiornamento dell'uscita DAC per **MAP** (in base a RPM, TPS, tasso di variazione RPM).
- Calcolo e aggiornamento dell'uscita DAC per **CLT** (in base a tempo di accensione, RPM, stato della ventola letto dall'ECU).
- Gestione della comunicazione seriale (comandi ArduStim per cambiare ruota fonica o ricevere configurazioni).

> **[!IMPORTANT] Fix verificato su hardware (2026-09):** sulla ISR del timer `onCrankTimer()` l'uso di `digitalWrite()` NON aggiorna i pin GPIO su ESP32 Arduino Core v3.3.5 (i segnali Crank/Cam restavano a 0V anche a motore avviato). Risolto sostituendo `digitalWrite` con la **scrittura diretta ai registri GPIO** (`GPIO.out_w1ts` per HIGH, `GPIO.out_w1tc` per LOW), che funziona sempre dentro una ISR. Aggiunti gli include `soc/gpio_struct.h` e `soc/gpio_reg.h`. Dopo questa patch il segnale Crank/Cam commuta correttamente (tensione media ~1.6V = square 3.3V a duty ~50%) e Speeduino/Mega2560 legge i giri.

#### [NEW] [wheel_defs.h](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/firmware/wheel_defs.h)
Le definizioni delle ruote foniche importate da ArduStim (es. 60-2, 36-1, ecc.) adattate per ESP32.

#### [NEW] [comms.h](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/firmware/comms.h) & [comms.cpp](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/firmware/comms.cpp)
Modulo di comunicazione seriale compatibile con l'interfaccia ArduStim per consentire la selezione del pattern e configurazioni tramite PC.

---

### Componente 2: Documentazione Hardware & Repository
#### [NEW] [README.md](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/README.md)
Documentazione completa per GitHub, contenente:
- Descrizione del progetto.
- Schema a blocchi dell'hardware (Alimentazione 12V -> Regolatori 5V e 3.3V, Level Shifter, DAC 0-3.3V amplificati a 0-5V per MAP e CLT).
- Pinout del connettore Dupont a 40 pin per l'interfaccia con Speeduino.
- Guida all'uso e alla calibrazione.

#### [NEW] [dupont_pinout.md](file:///C:/Users/nicola/Desktop/Engine%20Simulator%202.0/doc/dupont_pinout.md)
Tabella dettagliata delle connessioni tra ESP32, Level Shifter e connettore Dupont a 40 pin di Speeduino.

---

## Logiche Fisiche Simulate

### 1. Inerzia RPM da TPS
- Quando il motore è acceso, il regime target è proporzionale all'apertura del TPS.
- Gli RPM effettivi seguono il target con un ritardo controllato da un coefficiente di inerzia (simulando la massa del volano):
  $$\text{RPM}_{\text{curr}} = \text{RPM}_{\text{curr}} + (\text{RPM}_{\text{target}} - \text{RPM}_{\text{curr}}) \times K_{\text{inerzia}}$$

### 2. Pressione Collettore (MAP)
- A motore spento: MAP = Pressione Atmosferica (~4.0V - 4.5V sul sensore a seconda del sensore MAP simulato).
- In Idle (TPS basso, RPM stabili): MAP cala drasticamente (forte vuoto, ~1.0V - 1.5V).
- In Accelerazione (TPS alto, RPM in salita): MAP sale rapidamente verso la pressione atmosferica (~4.0V) prima che gli RPM aumentino del tutto.
- In Decelerazione (TPS chiuso, RPM alti): MAP scende al minimo storico (~0.5V - 0.8V).

### 3. Temperatura Refrigerante (CLT)
- A motore spento da tempo: CLT = Temperatura Ambiente.
- A motore acceso: CLT sale gradualmente verso una temperatura di esercizio (~90°C). La velocità di riscaldamento dipende dagli RPM.
- Se CLT supera la soglia della ventola (~95°C), Speeduino attiva l'uscita Ventola. L'ESP32 rileva questo input e aumenta il coefficiente di raffreddamento, facendo calare o stabilizzare la temperatura.
- A motore spento: CLT scende lentamente verso la temperatura ambiente seguendo la legge del raffreddamento di Newton.

---

## FASE 2 - Level Shifter Ingressi + Monitoraggio Timing Iniettori/Candele

> [!NOTE]
> Questa sezione descrive l'implementazione pianificata. **Il monitoraggio PWM di iniettori/candele NON è ancora implementato** nel codice (gli 8-9 ingressi non sono ancora gestiti). Il firmware base (generazione Crank/Cam + modello motore) è invece funzionante e verificato su Speeduino.

### Obiettivo

Monitorare i segnali **PWM di uscita della Speeduino** (4 iniettori + 4 candele + ventola) per misurare i **tempi di apertura iniettori (ms) e l'anticipo candela (°)**. Questo consente di verificare in tempo reale che la ECU e le correzioni (lambda, CLT, ecc.) effettivamente modifichino i parametri.

### 1. Corsia dati: i segnali Speeduino da leggere (IDC 40 pin V0.4)

| # | Segnale | Pin IDC | Natura |
|:-:|:--------|:-------:|:-------|
| 1 | Injector 1 | 1 | PWM low-side, attivo basso |
| 2 | Injector 2 | 2 | PWM low-side, attivo basso |
| 3 | Injector 3 | 3 | PWM low-side, attivo basso |
| 4 | Injector 4 | 5 | PWM low-side, attivo basso |
| 5 | Ignition 1 | 7 | PWM low-side, attivo basso |
| 6 | Ignition 2 | 34 | PWM low-side, attivo basso |
| 7 | Ignition 3 | 33 | PWM low-side, attivo basso |
| 8 | Ignition 4 | 8 | PWM low-side, attivo basso |
| 9 | Fan (retroazione ECU) | — | Segnale comando ventola, letto via CD4050-A |

Questi segnali sono a **5V/12V logica Speeduino**. Per leggerli con l'ESP32 (3.3V) serve una **rete di protezione + buffer** (CD4050BE) su ciascun ingresso.

### 2. Level Shifter: componente e necessita'

**Motivo:** Gli output iniettori/candele di Speeduino sono **low-side**: il pin IDC oscilla tra ~0V (ON) e fino a **12V** (batteria/Vdrive) quando OFF. Leggere direttamente questo pin supera i 3.3V dell'ESP32 e danneggerebbe sia il GPIO che qualsiasi level shifter a 5V (Vmax ~5.5V). Gli ESP32 DevKitC **non sono 5V/12V tolerant** → serve protezione dedicata.

**Componente scelto: 2x CD4050BE** (Hex Non-Inverting Buffer, 6 canali l'uno).

| Proprieta' | Valore |
|:-----------|:-------|
| Tolleranza ingresso | **VDD max 18V** → sopporta i 12V Speeduino senza danno |
| Canali | 6 per chip (ne servono 8 ⇒ **2 chip**) |
| Alimentazione (decisione) | **VDD = 3.3V**, GND condiviso con ESP32 |
| Uscita | Livello VDD = **3.3V**, già compatibile direttamente con GPIO ESP32 |
| Direzione | Unidirezionale (ingresso → uscita), perfetto per la lettura |

**Cablaggio (2 chip CD4050BE, 1 chip per sistema):**

```
Speeduino IDC (iniettori/candele/ventola, 0-12V)
        |   (ogni segnale su un ingresso)
        ▼
  [ CD4050BE-A  = IGNITION  (4 candele: IGN1-4 + FAN) ]
  [ CD4050BE-B  = INJECTION (4 iniettori: INJ1-4)      ]
        |  (VDD=3.3V, GND=GND ESP32)
        ▼
  GPIO ESP32 (9 ingressi, livello 3.3V)
```

> Distribuzione finale verificata: **CD4050BE-A = IGNITION** (buffer 9→10, 11→12, 14→15, 3→2 + FAN 7→6), **CD4050BE-B = INJECTION** (buffer 5→4, 7→6, 9→10, 11→12). Vedi sezioni 2.3-2.4 per i dettagli pin per pin.

> La VDD a 3.3V fa sì che l'uscita dei CD4050 sia già a livello logico compatibile con l'ESP32, eliminando ulteriori partitori sul cammino dei segnali di monitoraggio.

#### 2.1 Rete di protezione ingresso (per ciascuno degli 8 canali)

Prima dell'ingresso del CD4050 si inserisce una rete di protezione che limita la corrente e **clampa la tensione** sotto la soglia logica. Il CD4050 in sè regge 12V, ma la rete protegge da errori di cablaggio/spike e mantiene l'ingresso a livello pulito.

**Schema per canale (TH - Through Hole):**

```
Speeduino IDC ──[R 2.2k 1/4W]──┬──────> CD4050 IN
 (iniettore/candela)           │
                          Zener 3.6V
                          (BZX55C3V6, DO-35)
                               │
                              GND
```

| Componente | Valore | Package TH | Ruolo |
|:-----------|:-------|:-----------|:------|
| R serie | **2.2kΩ** 1/4W | TH | Limita corrente verso lo Zener e l'ingresso CD4050 |
| Zener | **3.6V** (`BZX55C3V6`) | **DO-35 (TH)** | Clampa la sovratensione appena sopra 3.3V, protegge ingresso e GPIO |

**Nota sulla disponibilità (TH):** il **BZX55C3V6** è in contenitore **DO-35 (TH**, 2 pin, 0.5W) — facile da trovare. Valori TH equivalenti se il 3.6V non fosse disponibile:

| Alternativa | Tensione | Package TH |
|:-----------|:--------:|:-----------|
| 1N5226B | 3.3V | DO-35 |
| BZX55C3V6 | 3.6V | DO-35 (scelto) |
| 1N5228B | 3.9V | DO-35 |

> Lo **Zener 3.6V** (e non 5.1V) è stato scelto perché clampando appena sopra i 3.3V protegge davvero l'ingresso, dato che il CD4050 è alimentato a VDD=3.3V. Un clamp a 5.1V sarebbe ridondante/parziale. I diodi Schottky (BAT85/1N5819) non sono Zener e non fanno clamp di soglia: scartati.

**Quantità rete protezione:** 9 canali ⇒ 9× R2.2k + 9× BZX55C3V6 (tutti TH).

#### 2.2 Simbolo KiCad per il CD4050BE

> Il CD4050B **non è presente** nelle librerie standard di KiCad 10 (il simbolo "Hex Buffer" generico esistente ha una pinout diversa e va scartato). La libreria Digi-Key esiste ma usa il formato `.lib` legacy, poco compatibile con KiCad 9/10 per un singolo componente.

**Soluzione consigliata: creare un simbolo custom** in una libreria di progetto (`.kicad_sym`) con la pinout confermata da datasheet TI (CD4049UB/CD4050B).

**Pinout CD4050BE (PDIP-16, da datasheet TI):**

| Pin | Funzione | Pin | Funzione |
|:---:|:---------|:---:|:---------|
| 1 | VDD | 16 | **NC** (non connesso) |
| 2 | OUT (G) | 15 | OUT (L) |
| 3 | IN (A) | 14 | IN (F) |
| 4 | OUT (H) | 13 | **NC** (non connesso) |
| 5 | IN (B) | 12 | OUT (K) |
| 6 | OUT (I) | 11 | IN (E) |
| 7 | IN (C) | 10 | OUT (J) |
| 8 | VSS (GND) | 9 | IN (D) |

> **Importante (dal datasheet TI, SCHS046):** i pin **13 e 16 sono NC** (non connessi internamente).  
> I **6 buffer validi** sono sulle coppie (IN→OUT): **(3→2), (5→4), (7→6), (9→10), (11→12), (14→15)**.  
> Ingressi: A=3, B=5, C=7, D=9, E=11, F=14 · Uscite: G=2, H=4, I=6, J=10, K=12, L=15.  
> **NON usare i pin 13 e 16 come ingresso/uscita.**

**Dati chiave CD4050B (da datasheet):**
- Buffer **non-inverting**, **6 canali** (hex)
- **Pin 13 e 16 = NC** — usare solo le 6 coppie valide sopra
- VDD range **3–18V** (sopporta i 12V)
- Corrente ingresso max 1µA, TH tollerante
- Package TH: **PDIP-16** → footprint `Package_DIP:DIP-16_W7.62mm`

**Procedura in KiCad:**
1. `File → New → Symbol Library` → crea `engine_sim.kicad_sym` nella cartella progetto
2. `File → New Symbol` → nome `CD4050BE`
3. Aggiungi i pin secondo la tabella sopra (6 coppie valide + VDD pin1 + VSS pin8; pin 13/16 NC)
4. Assegna footprint `Package_DIP:DIP-16_W7.62mm`
5. Salva

**Footprint di riferimento (per CD4050BE TH):** `Package_DIP:DIP-16_W7.62mm`

### 2.3 Assegnazione dei 2 chip (CD4050BE-A e CD4050BE-B)

I 2 chip CD4050BE sono divisi **per sistema**: **chip A = Ignition (4 candele)**, **chip B = Injection (4 iniettori)**. Ogni chip usa solo le coppie valide (le 6 di datasheet TI); i pin 13 e 16 NC non vengono mai usati. Entrambi i chip: **VDD(pin1)=3.3V**, **VSS(pin8)=GND**.

**CD4050BE-A (IGNITION + FAN — 4 candele + ventola):**

| Segnale | Pin IDC | IN CD4050-A | OUT CD4050-A | GPIO ESP32 | PIN DevKitC |
|:--------|:-------:|:----------:|:------------:|:----------:|:-----------:|
| IGN1 | 7 | 9 | 10 | GPIO 19 | 31 |
| IGN2 | 34 | 11 | 12 | GPIO 21 | 33 |
| IGN3 | 33 | 14 | 15 | GPIO 22 | 36 |
| IGN4 | 8 | 3 | 2 | GPIO 23 | 37 |
| FAN | — | 7 | 6 | GPIO 27 | 11 |

**CD4050BE-B (INJECTION — 4 iniettori):**

| Iniettore | Pin IDC | IN CD4050-B | OUT CD4050-B | GPIO ESP32 | PIN DevKitC |
|:----------|:-------:|:----------:|:------------:|:----------:|:-----------:|
| INJ1 | 1 | 5 | 4 | GPIO 15 | 23 |
| INJ2 | 2 | 7 | 6 | GPIO 16 | 27 |
| INJ3 | 3 | 9 | 10 | GPIO 17 | 28 |
| INJ4 | 5 | 11 | 12 | GPIO 18 | 30 |

> Ogni ingresso (IN) viaggia attraverso la **rete di protezione** (R 2.2k + Zener 3.6V). Il **chip A** usa 5 dei 6 buffer valevi (4 candele + Fan), il **chip B** ne usa 4. Pin 13 e 16 NC su entrambi, mai usati; i buffer liberi restano scollegati.



#### 2.4 Tabella di connessione per sistema (Ignition / Injection)

Vista consolidata organizzata per **sistema** — **1 chip per sistema** (tutti i valori in pin fisici CD4050 da datasheet TI):

**CD4050BE-A = IGNITION + FAN (4 candele + ventola):**

| Segnale | Pin IDC | IN CD4050-A | OUT CD4050-A | GPIO ESP32 | PIN DevKitC |
|:--------|:-------:|:----------:|:------------:|:----------:|:-----------:|
| IGN1 | 7 | 9 | 10 | GPIO 19 | 31 |
| IGN2 | 34 | 11 | 12 | GPIO 21 | 33 |
| IGN3 | 33 | 14 | 15 | GPIO 22 | 36 |
| IGN4 | 8 | 3 | 2 | GPIO 23 | 37 |
| FAN | — | 7 | 6 | GPIO 27 | 11 |

**CD4050BE-B = INJECTION (4 iniettori):**

| Iniettore | Pin IDC | IN CD4050-B | OUT CD4050-B | GPIO ESP32 | PIN DevKitC |
|:----------|:-------:|:----------:|:------------:|:----------:|:-----------:|
| INJ1 | 1 | 5 | 4 | GPIO 15 | 23 |
| INJ2 | 2 | 7 | 6 | GPIO 16 | 27 |
| INJ3 | 3 | 9 | 10 | GPIO 17 | 28 |
| INJ4 | 5 | 11 | 12 | GPIO 18 | 30 |

> Il **chip A** usa 5 buffer (4 candele + Fan), il **chip B** ne usa 4. Pin 13 e 16 NC su entrambi, mai usati. VDD(pin1)=3.3V, VSS(pin8)=GND su entrambi.

### 3. Identificazione dei 9 ingressi ESP32 (GPIO)

Firmware attuale usa: GPIO 4, 34, 27, 25, 26, 12, 13, 14 (liberi tutti gli altri).  
Per il monitoraggio PWM servono GPIO **con interrupt/input capture** (tutti i GPIO supportano interrupt su ESP32). Scelta proposta (canale RTC libero, nessun conflitto con DAC/ADC usati):

| Funzione | GPIO | PIN DevKitC | Note |
|:---------|:----:|:-----------:|:-----|
| Injector 1 | **GPIO 15** | PIN 23 | INPUT_PULLUP |
| Injector 2 | **GPIO 16** | PIN 27 | INPUT_PULLUP |
| Injector 3 | **GPIO 17** | PIN 28 | INPUT_PULLUP |
| Injector 4 | **GPIO 18** | PIN 30 | INPUT_PULLUP |
| Ignition 1 | **GPIO 19** | PIN 31 | INPUT_PULLUP |
| Ignition 2 | **GPIO 21** | PIN 33 | INPUT_PULLUP |
| Ignition 3 | **GPIO 22** | PIN 36 | INPUT_PULLUP |
| Ignition 4 | **GPIO 23** | PIN 37 | INPUT_PULLUP |
| Fan (via CD4050-A) | **GPIO 27** | PIN 11 | INPUT_PULLUP |

> Questi GPIO non confliggono con i pin di I/O già assegnati (TPS, MAP, CLT, Crank, Cam1, Cam2, Fan, Start/Stop) — il GPIO 27/Fan è un'uscita del CD4050-A (buffer 7→6) che rientra già tra i GPIO usati dal firmware.
>
> **Nota mappatura pin fisici DevKitC (verificata dalla scheda):** GPIO15→PIN23, GPIO16→PIN27, GPIO17→PIN28, GPIO18→PIN30, GPIO19→PIN31, GPIO21→PIN33, GPIO22→PIN36, GPIO23→PIN37, GPIO27→PIN11. I numeri PIN in tutte le tabelle fanno riferimento a questa mappatura (non alla piedinatura standard di altri DevKitC).

### 4. Algoritmo di timing da implementare

Si userà una tecnica **"input capture su ISR + timestamp via micros()"** per misurare i parametri di ciascun canale senza bloccare il loop principale (che deve restare a bassa latenza per il modello motore).

#### 4.1 Misura tempi iniettore (duty / on-time)
Per ciascun canale iniettore si rileva il fronte di **salita e discesa** del segnale e si calcola in ISR:
- `onTime_us[ch]` = durata dell'impulso ATTIVO (apertura iniettore) in microsecondi → convertito in ms.
- `period_us[ch]` = periodo completo del ciclo (tra due iniezioni consecutive).
- **Frequenza iniezione** = `1 / period` (raddoppia nel semi-sequenziale/batch).

Questo dà direttamente il **tempo di iniezione** (in ms) che la ECU sta applicando, e permette di vedere le correzioni (incremento a CLT freddo, arricchimento in accelerazione, ecc.).

#### 4.2 Misura anticipo candela (spark timing)
Per la candela occorre non solo il duty ma la **posizione temporale dell'impulso rispetto al riferimento Crank**. Poiché l'ESP32 genera la ruota fonica (ed è quindi il "master", conosce il giro motore), si può:
- Rilevare l'impulso di candela sul canale ignition.
- Confrontare il timestamp con l'ultimo fronte del **Crank** (già generato localmente).
- Calcolare l'angolo in gradi = `(Δt / periodo_giro) × 360°`.

**Risultato:** anticipo candela in **gradi BTDC**, che è il parametro chiave che la ECU modifica con MAP/RPM (advance table).

#### 4.3 Su quali core/priorita' eseguire
- Gli 8 interrupt di input capture girano sulla ISR (istante di trigger, solo `micros()` e store in struttura volatile). **Fase 1 (generazione Crank/Cam)** resta invariata sul timer.
- Il **calcolo** dei valori finali (ms, gradi, frequenze) avviene nel loop (priorita' bassa) e viene solo **esposto** in memoria condivisa.
- Il **WiFi/web server** (fase successiva) e il seriale leggono i valori calcolati → niente conflitto di timing.

#### 4.4 Strutture dati (da aggiungere a `globals.h`)
```cpp
struct injectorChannel {
  volatile uint32_t onTimeUs;   // durata apertura (us)
  volatile uint32_t periodUs;   // periodo ciclo (us)
  volatile uint32_t lastRise;   // timestamp fronte di salita (micros)
  volatile uint32_t lastFall;   // timestamp fronte di discesa (micros)
  bool active;                  // uscita attiva?
};

struct ignitionChannel {
  volatile uint32_t sparkTimeUs;   // durata impulso candela
  volatile uint16_t advanceDeg;    // anticipo in gradi BTDC
  bool active;
};

extern injectorChannel injectors[4];
extern ignitionChannel ignitions[4];
```

#### 4.5 Interrupt handler (esempio concettuale, NON ancora nel codice)
```cpp
void IRAM_ATTR injISR() {
  uint32_t now = micros();
  bool pinState = digitalRead(injPin);
  if (pinState == LOW) {            // attivo basso: fronte di discesa = apertura
    inj.onTimeUs = now - inj.lastRise;
    inj.periodUs = now - inj.lastRise + inj.lastPeriod...; // gestendo frazioni
    inj.lastRise = now;
  }
}
```

### 5. Stub / placeholder nel codice attuale (da non modificare ancora)
- Nessuna modifica al software finche' non si approva il piano e si definisce la sequenza di sviluppo.
- I nuovi file/simboli saranno introdotti in una tranche successiva senza rompere la compilazione attuale (che e' verificata).

### 6. Verifica pianificata (Fase 2)
1. **Oscilloscopio sui 3 level shifter**: verificare che i segnali 5V → 3.3V siano integri (niente undershoot).
2. **Comparazione tempi**: confrontare i ms di iniezione misurati dal firmware con quelli mostrati da TunerStudio.
3. **Verifica anticipo**: confrontare i gradi calcolati con la spark advance table di Speeduino.
4. **Stabilità**: garantire che gli 8 ISR non interferiscano con la generazione Crank/Cam (test a RPM alti).

---

## Verification Plan

### Automated/Unit Tests
Non essendoci un banco di prova hardware automatico integrato nel computer locale, simuleremo le letture analogiche e i comportamenti fisici tramite un modulo di simulazione software attivabile via console seriale.

### Manual Verification
1. **Analisi su Oscilloscopio o Analizzatore Logico:** Verifica della corretta generazione dei segnali di Crank e Cam su GPIO selezionati.
2. **Test Seriale:** Connessione tramite terminale seriale (es. Arduino Serial Monitor) a 115200 baud per validare l'invio dei comandi di ArduStim e verificare le risposte.
3. **Verifica dei DAC:** Lettura delle uscite analogiche di MAP e CLT in risposta a variazioni del TPS (simulato) o del pulsante di Start.
4. **[x] Test Crank/Cam su Speeduino (eseguito):** completo — il firmware genera Crank (GPIO12)/Cam1 (GPIO13)/Cam2 (GPIO14) a 3.3V e Speeduino (Mega2560) legge correttamente i giri. Richiedeva la fix ai registri GPIO nella ISR (vedi sezione Componente 1). Il pulsante Start/Stop (GPIO4) avvia il motore fino a ~850 rpm idle; sulle uscite Crank/Cam si misura ~1.6V di media (square 3.3V duty ~50%).
