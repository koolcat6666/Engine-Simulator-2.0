/*
 * timing.h - Input capture e calcolo timing iniettori/candele (FASE 2)
 *
 * Misura i segnali PWM di uscita della Speeduino (4 iniettori + 4 candele + ventola)
 * letti attraverso i 2 CD4050BE (chip A = IGNITION+FAN, chip B = INJECTION).
 *
 * Tecnica: "input capture su ISR + timestamp via micros()".
 *   - Le ISR salvano solo timestamp in strutture volatile (bassissima latenza).
 *   - Il calcolo dei valori finali (ms, gradi, Hz) avviene nel loop() (priorita' bassa).
 *
 * NOTA: i segnali Speeduino sono LOW-SIDE attivi bassi. Dopo il CD4050 (non-inverting,
 * VDD=3.3V) il livello logico ha la stessa polarita' del segnale originale: LOW = attivo.
 */

#ifndef __TIMING_H__
#define __TIMING_H__

#include <Arduino.h>
#include "globals.h"

// --- MAPPATURA GPIO (dalla FASE 2, pinout CD4050BE verificato) ---
#define PIN_INJ1   15
#define PIN_INJ2   16
#define PIN_INJ3   17
#define PIN_INJ4   18
#define PIN_IGN1   19
#define PIN_IGN2   21
#define PIN_IGN3   22
#define PIN_IGN4   23

// Fronte di riferimento Crank per il calcolo anticipo candela.
// Scritto dalla ISR del timer Crank (vedi onCrankTimer nel .ino).
extern volatile uint32_t lastCrankEdgeUs;

// Inizio del pattern (edge_counter==0): riferimento angolare del ciclo.
// Scritto dalla ISR del timer Crank (vedi onCrankTimer nel .ino).
extern volatile uint32_t revStartUs;

void timingSetup();
void timingUpdate();      // calcola valori finali dai timestamp grezzi (chiamare nel loop)
void timingTest(bool on); // autotest: genera PWM noti su GPIO32/33 (jumper verso INJ1/IGN1)

#endif
