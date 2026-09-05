/*
 * timing.cpp - Input capture e calcolo timing iniettori/candele (FASE 2)
 *
 * Misura i PWM di uscita della Speeduino (4 iniettori + 4 candele + ventola).
 * Segnali LOW-SIDE attivi bassi: dopo il CD4050BE non-inverting il livello rimane
 * LOW = attivo.
 *
 * - Le ISR (IRAM_ATTR, accesso direttamente a GPIO.in per evitare i problemi di
 *   digitalRead/digitalWrite in ISR su ESP32 Arduino Core v3.3.5) salvano SOLO
 *   timestamp (bassissima latenza, nessun blocco del loop di simulazione).
 * - Il calcolo dei valori finali (ms, gradi, Hz, duty) avviene in timingUpdate()
 *   chiamato dal loop() a bassa priorita'.
 *
 * Riferimento per l'anticipo candela: l'ESP32 e' il "master" della ruota fonica,
 * quindi conosce l'istante di ogni fronte Crank (lastCrankEdgeUs, scritto dalla
 * ISR del timer). L'anticipo = (delta rispetto al fronte Crank) +/- swap.
 */

#include "timing.h"
#include "globals.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"

extern wheels Wheels[];

// Lati dei pin reali (mappatura FASE 2 verificata)
static const uint8_t injPin[4] = { PIN_INJ1, PIN_INJ2, PIN_INJ3, PIN_INJ4 };
static const uint8_t ignPin[4] = { PIN_IGN1, PIN_IGN2, PIN_IGN3, PIN_IGN4 };

// Timestamp grezzi scritti SOLO dalle ISR (volatile)
struct injRaw {
  volatile uint32_t lastStartUs;   // ultimo fronte di discesa (inizio apertura)
  volatile uint32_t lastRiseUs;    // ultimo fronte di salita (fine apertura)
  volatile uint32_t periodUs;      // periodo tra due iniezioni
  volatile uint32_t onTimeUs;      // tempo apertura (on-time)
};
static injRaw injRawData[4];

struct ignRaw {
  volatile uint32_t sparkStartUs;  // inizio impulso candela (fronte discesa)
  volatile uint32_t sparkEndUs;    // fine impulso candela (fronte salita)
  volatile uint32_t lastRsUs;      // ultimo fronte salita (per il periodo)
  volatile uint32_t dwellUs;       // durata impulso candela (dwell)
  volatile uint32_t periodUs;      // periodo tra due candele
};
static ignRaw ignRawData[4];

// Fronte di riferimento Crank (scritto dalla ISR del timer nel .ino)
volatile uint32_t lastCrankEdgeUs = 0;

// Inizio del pattern (edge_counter==0): riferimento angolare del ciclo
volatile uint32_t revStartUs = 0;

// --- ISR INIETTORI (attivo basso: DISCESA = apertura, SALITA = chiusura) ---
#define MAKE_INJ_ISR(i)                                                                \
void IRAM_ATTR injISR##i() {                                                           \
  uint32_t now = micros();                                                             \
  if (GPIO.in & (1ULL << injPin[i])) {  /* SALITA: fine apertura */                   \
    injRawData[i].onTimeUs = now - injRawData[i].lastStartUs;                          \
    injRawData[i].periodUs = now - injRawData[i].lastRiseUs;                           \
    injRawData[i].lastRiseUs = now;                                                    \
  } else {                          /* DISCESA: inizio apertura */                     \
    injRawData[i].lastStartUs = now;                                                   \
  }                                                                                    \
}

MAKE_INJ_ISR(0)
MAKE_INJ_ISR(1)
MAKE_INJ_ISR(2)
MAKE_INJ_ISR(3)

// --- ISR CANDELE (attivo basso: DISCESA = inizio spark, SALITA = fine spark) ---
#define MAKE_IGN_ISR(i)                                                                \
void IRAM_ATTR ignISR##i() {                                                           \
  uint32_t now = micros();                                                             \
  if (GPIO.in & (1ULL << ignPin[i])) {  /* SALITA: fine spark */                      \
    ignRawData[i].dwellUs = now - ignRawData[i].sparkStartUs;                          \
    ignRawData[i].periodUs = now - ignRawData[i].lastRsUs;                             \
    ignRawData[i].lastRsUs = now;                                                      \
    ignRawData[i].sparkEndUs = now;                                                    \
  } else {                          /* DISCESA: inizio spark */                        \
    ignRawData[i].sparkStartUs = now;                                                  \
  }                                                                                    \
}

MAKE_IGN_ISR(0)
MAKE_IGN_ISR(1)
MAKE_IGN_ISR(2)
MAKE_IGN_ISR(3)

// --- SETUP: pin in ingresso con pull-up e attachInterrupt su CHANGE ---
void timingSetup() {
  // Configurazione pin (tutti con pull-up interno)
  pinMode(PIN_INJ1, INPUT_PULLUP);
  pinMode(PIN_INJ2, INPUT_PULLUP);
  pinMode(PIN_INJ3, INPUT_PULLUP);
  pinMode(PIN_INJ4, INPUT_PULLUP);
  pinMode(PIN_IGN1, INPUT_PULLUP);
  pinMode(PIN_IGN2, INPUT_PULLUP);
  pinMode(PIN_IGN3, INPUT_PULLUP);
  pinMode(PIN_IGN4, INPUT_PULLUP);

  // Interrupt su ogni fronte (CHANGE)
  attachInterrupt(digitalPinToInterrupt(PIN_INJ1), injISR0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_INJ2), injISR1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_INJ3), injISR2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_INJ4), injISR3, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_IGN1), ignISR0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_IGN2), ignISR1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_IGN3), ignISR2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_IGN4), ignISR3, CHANGE);

  // Azzera le strutture pubbliche
  for (uint8_t i = 0; i < 4; i++) {
    injectors[i].active      = false;
    injectors[i].onTimeMs    = 0.0f;
    injectors[i].frequencyHz = 0.0f;
    injectors[i].dutyPct     = 0;

    ignitions[i].active      = false;
    ignitions[i].dwellMs     = 0.0f;
    ignitions[i].advanceDeg  = 0;
  }
}

// --- AUTOTEST: generatore PWM su GPIO32/33 per verificare la catena di acquisizione.
//   GPIO32 -> INJ1 (50 Hz, on-time 4.0 ms -> duty 20%)
//   GPIO33 -> IGN1 (100 Hz, dwell 2.0 ms)
//   I segnali Speeduino sono attivi-bassi: il calcolo quindi usa il tempo LOW.
//   ledcWrite = parte HIGH; usiamo 80% HIGH -> 20% LOW.
void timingTest(bool on) {
  if (on) {
    ledcAttach(32, 50, 12);   ledcWrite(32, 3276);  // INJ1: periodo 20ms, LOW 4ms
    ledcAttach(33, 100, 12);  ledcWrite(33, 3276);  // IGN1: periodo 10ms, LOW 2ms
  } else {
    ledcDetach(32);
    ledcDetach(33);
  }
}

// --- CALCOLO VALORI FINALI (loop, a bassa priorita') ---
void timingUpdate() {
  // Soglia minima RPM per avere senso fisico nel calcolo dell'anticipo
  float rpm = currentStatus.rpm;
  bool engineRunning = (rpm >= 10);

  // Riferimento Crank aggiornato dall'ISR del timer
  uint32_t crankUs = lastCrankEdgeUs;
  // Inizio del pattern: riferimento angolare del ciclo (per l'anticipo)
  uint32_t revStart = revStartUs;

  // @Injectors
  for (uint8_t i = 0; i < 4; i++) {
    volatile injRaw *r = &injRawData[i];

    if (r->periodUs > 0 && r->onTimeUs > 0) {
      injectors[i].active = true;
      injectors[i].onTimeMs = (float)r->onTimeUs / 1000.0f;
      float periodSec = (float)r->periodUs / 1000000.0f;
      injectors[i].frequencyHz = (periodSec > 0.0f) ? (1.0f / periodSec) : 0.0f;
      // duty = on-time / periodo
      float duty = (periodSec > 0.0f) ? (100.0f * (float)r->onTimeUs / (float)r->periodUs) : 0.0f;
      if (duty > 100.0f) duty = 100.0f;
      injectors[i].dutyPct = (uint16_t)duty;
    } else {
      injectors[i].active = false;
    }
  }

  // @Ignitions + anticipo
  for (uint8_t i = 0; i < 4; i++) {
    volatile ignRaw *r = &ignRawData[i];

    if (r->dwellUs > 0) {
      ignitions[i].active = true;
      ignitions[i].dwellMs = (float)r->dwellUs / 1000.0f;
    } else {
      ignitions[i].active = false;
    }

    // Anticipo: angolo dello spark rispetto all'inizio del pattern (revStartUs),
    // normalizzato al ciclo. Con wrap a 32 bit la differenza è sempre positiva:
    // se lo spark arriva PRIMA dell'inizio del ciclo (ciclo successivo) aggiungiamo
    // un periodo per mantenere l'angolo nel range [0, wheel_degrees).
    if (engineRunning && revStart != 0 && ignitions[i].active) {
      float degreesPerRev = (float)Wheels[config.wheel].wheel_degrees;
      float periodRevUs = (degreesPerRev / 360.0f) * 60000000.0f / rpm;
      // Differenza firmata (wrap-aware): negativa se lo spark precede l'inizio del ciclo
      int32_t diff = (int32_t)(r->sparkStartUs - revStart);
      float delta = (float)diff;
      if (delta < 0.0f) delta += periodRevUs;
      if (delta >= periodRevUs) delta -= periodRevUs;
      ignitions[i].advanceDeg = (int16_t)((delta / periodRevUs) * degreesPerRev);
    } else {
      ignitions[i].advanceDeg = 0;
    }
  }
}
