#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <Arduino.h>
#include "wheel_defs.h"

// --- MODALITA' DI GESTIONE RPM (compatibilità ArduStim) ---
#define POT_RPM 0
#define FIXED_RPM 1
#define LINEAR_SWEPT_RPM 2

// --- STRUTTURE DATI CONDIVISE ---
struct configTable {
  uint8_t version;
  uint8_t wheel;
  uint8_t mode;
  uint16_t fixed_rpm;
  uint16_t sweep_low_rpm;
  uint16_t sweep_high_rpm;
  uint16_t sweep_interval;
  bool useCompression;
  uint8_t compressionType;
  uint16_t compressionRPM;
  uint16_t compressionOffset;
  bool compressionDynamic;
} __attribute__ ((packed));

struct status {
  uint16_t base_rpm;
  uint16_t compressionModifier;
  uint16_t rpm;
};

extern struct configTable config;
extern struct status currentStatus;

// --- STATO MOTORE DI SIMULAZIONE (condiviso con web server / /api/data) ---
enum MotorState {
  ENGINE_OFF,
  ENGINE_CRANKING,
  ENGINE_RUNNING,
  ENGINE_STOPPING
};

extern MotorState engineState;
extern float currentRpmFloat;
extern float tpsValue;          // apertura TPS normalizzata 0.0 - 1.0
extern float engineTemp;        // CLT in °C
extern float targetTemp;        // temperatura a regime in °C
extern float currentMapKpa;     // pressione collettore in kPa
extern bool fanActive;          // stato ventola letta dalla ECU

// --- TIMING INIETTORI / CANDELE (FASE 2 - monitoraggio PWM via CD4050BE) ---
// Valori misurati dalle ISR in timing.cpp; un canale e' "active" solo quando il
// segnale della Speeduino supera almeno un ciclo completo misurato.
struct injectorTiming {
  bool active;                  // canale con segnale rilevato?
  float onTimeMs;               // tempo apertura iniettore (ms)
  float frequencyHz;            // frequenza iniezione (Hz)
  uint16_t dutyPct;             // duty cycle in % (0-100)
};

struct ignitionTiming {
  bool active;                  // canale con segnale rilevato?
  float dwellMs;                // durata impulso candela (ms)
  int16_t advanceDeg;           // anticipo candela in gradi BTDC
};

extern injectorTiming injectors[4];
extern ignitionTiming ignitions[4];

// --- UTILITA' CONNESSA AL SISTEMA ---
extern uint8_t connectedClients; // numero di client wifi collegati alla dashboard

#endif
