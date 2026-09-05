/*
 * comms.cpp - Parser comandi seriale (protocollo ArduStim)
 * Permette di configurare ruota fonica e modalita' RPM via USB da desktop ArduStim GUI.
 * Il motore e' controllato via potenziometro TPS quando in modalita' POT_RPM.
 */

#include "globals.h"
#include "comms.h"

extern wheels Wheels[];

// Variabili volatili esterne
extern volatile uint16_t edge_counter;
extern volatile uint8_t output_invert_mask;
extern void reset_new_OCR1A(uint32_t);

bool cmdPending;
byte currentCommand;

void serialSetup() {
  Serial.begin(115200);
  cmdPending = false;
}

void commandParser() {
  char buf[80];
  byte tmp_wheel;
  void* pnt_Config = &config;
  if (cmdPending == false) { currentCommand = Serial.read(); }

  switch (currentCommand)
  {
    case 'a':
      break;

    case 'c': // Ricezione dell'intero buffer di configurazione
      while(Serial.available() < (int)(sizeof(struct configTable)-1) ) {} // Attende tutti i byte
      for(uint8_t x=1; x<(sizeof(struct configTable)); x++)
      {
        *((uint8_t *)pnt_Config + x) = Serial.read();
      }
      break;

    case 'C': // Invio della configurazione corrente
      for(uint8_t x=0; x<sizeof(struct configTable); x++)
      {
        Serial.write(*((uint8_t *)pnt_Config + x));
      }
      break;
      
    case 'L': // Invio dei nomi delle ruote foniche
      for(byte x=0;x<MAX_WHEELS;x++)
      {
        strcpy_P(buf, Wheels[x].decoder_name);
        Serial.println(buf);
      }
      break;

    case 'n': // Numero totale delle ruote foniche
      Serial.println(MAX_WHEELS);
      break;

    case 'N': // Numero della ruota fonica corrente
      Serial.println(config.wheel);
      break;
    
    case 'p': // Numero massimo di transizioni della ruota corrente
      Serial.println(Wheels[config.wheel].wheel_max_edges);
      break;

    case 'P': // Invio del pattern completo della ruota corrente
      for(uint16_t x=0; x<Wheels[config.wheel].wheel_max_edges; x++)
      {
        if(x != 0) { Serial.print(","); }
        byte tempByte = pgm_read_byte(&Wheels[config.wheel].edge_states_ptr[x]);
        Serial.print(tempByte);
      }
      Serial.println("");
      Serial.println(Wheels[config.wheel].wheel_degrees);
      break;

    case 'R': // Invio RPM corrente
      Serial.println(currentStatus.rpm);
      break;

    case 'r': // Impostazione intervalli sweep RPM
      config.mode = LINEAR_SWEPT_RPM;
      while(Serial.available() < 6) {}
      config.sweep_low_rpm = word(Serial.read(), Serial.read());
      config.sweep_high_rpm = word(Serial.read(), Serial.read());
      config.sweep_interval = word(Serial.read(), Serial.read());
      break;

    case 's': // Salvataggio della configurazione (placeholder)
      break;

    case 'S': // Impostazione ruota fonica
      while(Serial.available() < 1) {} 
      tmp_wheel = Serial.read();
      if(tmp_wheel < MAX_WHEELS)
      {
        config.wheel = tmp_wheel;
        display_new_wheel();
      }
      break;

    case 'X': // Cambia ruota fonica alla successiva
      select_next_wheel_cb();
      strcpy_P(buf, Wheels[config.wheel].decoder_name);
      Serial.println(buf);
      break;

    default:
      break;
  }
  cmdPending = false;
}

uint16_t freeRam() {
  return (uint16_t)(ESP.getFreeHeap());
}

void toggle_invert_primary_cb() {
  output_invert_mask ^= 0x01;
}

void toggle_invert_secondary_cb() {
  output_invert_mask ^= 0x02;
}

void display_new_wheel() {
  reset_new_OCR1A(currentStatus.rpm);
  edge_counter = 0;
}

void select_next_wheel_cb() {
  if (config.wheel == (MAX_WHEELS-1))
    config.wheel = 0;
  else 
    config.wheel++;
  display_new_wheel();
}

void select_previous_wheel_cb() {
  if (config.wheel == 0)
    config.wheel = MAX_WHEELS-1;
  else 
    config.wheel--;
  display_new_wheel();
}
