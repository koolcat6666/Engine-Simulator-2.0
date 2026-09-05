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

#endif
