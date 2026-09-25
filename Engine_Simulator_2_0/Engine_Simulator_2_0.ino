/*
 * Engine Simulator 2.0 - Firmware per ESP32
 * Basato su ArduStim per la generazione dei pattern di ruote foniche.
 * Implementa la simulazione dinamica del motore (TPS, CLT, MAP, Start/Stop).
 *
 * Segnali generati dall'ESP32:
 *   - Crank (GPIO 12) / Cam1 (GPIO 13) / Cam2 (GPIO 14): Ruota fonica
 *   - CLT via DAC2 (GPIO 26): Temperatura acqua simulata
 *   - MAP via DAC1 (GPIO 25): Pressione collettore simulata
 *
 * Segnali gestiti manualmente (potenziometri verso Speeduino):
 *   - TPS: Potenziometro esterno, collegato a Speeduino + partitore verso ESP32 ADC
 *   - IAT: Potenziometro esterno, collegato direttamente a Speeduino
 *   - O2:  Potenziometro esterno, collegato direttamente a Speeduino
 */

#include <Arduino.h>
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "globals.h"
#include "comms.h"
#include "timing.h"
#include "web.h"

// --- CONFIGURAZIONE PIN ESP32 ---
#define PIN_START_STOP  4   // Pulsante accensione motore (con pull-up)
#define PIN_TPS_IN      34  // Ingresso analogico TPS (0-3.3V, da partitore sul potenziometro)

#define PIN_FAN_IN      27  // Lettura feedback ventola da ECU (0V = Attiva / pull-up)

// Uscite analogiche (DAC integrati di ESP32)
#define PIN_CLT_DAC     26  // Generazione tensione CLT analogica (DAC2)
#define PIN_MAP_DAC     25  // Generazione tensione MAP analogica (DAC1)

// Uscite digitali (Ruota Fonica)
#define PIN_CRANK_OUT   12  // Uscita primaria (Crank)
#define PIN_CAM1_OUT    13  // Uscita secondaria (Cam 1)
#define PIN_CAM2_OUT    14  // Uscita terziaria (Cam 2)

// --- DEFINIZIONI GLOBALI ---
struct configTable config;
struct status currentStatus;

// Array delle ruote foniche (definito in wheel_defs.h)
wheels Wheels[MAX_WHEELS] = {
  { dizzy_four_cylinder_friendly_name, dizzy_four_cylinder, 0.03333, 4, 360 },
  { dizzy_six_cylinder_friendly_name, dizzy_six_cylinder, 0.05, 6, 360 },
  { dizzy_eight_cylinder_friendly_name, dizzy_eight_cylinder, 0.06667, 8, 360 },
  { sixty_minus_two_friendly_name, sixty_minus_two, 1.0, 120, 360 },
  { sixty_minus_two_with_cam_friendly_name, sixty_minus_two_with_cam, 1.0, 240, 720 },
  { sixty_minus_two_with_halfmoon_cam_friendly_name, sixty_minus_two_with_halfmoon_cam, 1.0, 240, 720 },
  { thirty_six_minus_one_friendly_name, thirty_six_minus_one, 0.6, 72, 360 },
  { twenty_four_minus_one_friendly_name, twenty_four_minus_one, 0.5, 48, 360 },
  { four_minus_one_with_cam_friendly_name, four_minus_one_with_cam, 0.06667, 16, 720 },
  { eight_minus_one_friendly_name, eight_minus_one, 0.13333, 16, 360 },
  { six_minus_one_with_cam_friendly_name, six_minus_one_with_cam, 0.15, 36, 720 },
  { twelve_minus_one_with_cam_friendly_name, twelve_minus_one_with_cam, 0.6, 144, 720 },
  { fourty_minus_one_friendly_name, fourty_minus_one, 0.66667, 80, 360 },
  { dizzy_four_trigger_return_friendly_name, dizzy_four_trigger_return, 0.15, 9, 720 },
  { oddfire_vr_friendly_name, oddfire_vr, 0.2, 24, 360 },
  { optispark_lt1_friendly_name, optispark_lt1, 3.0, 720, 720 },
  { twelve_minus_three_friendly_name, twelve_minus_three, 0.4, 48, 360 },
  { thirty_six_minus_two_two_two_friendly_name, thirty_six_minus_two_two_two, 0.6, 72, 360 },
  { thirty_six_minus_two_two_two_h6_friendly_name, thirty_six_minus_two_two_two_h6, 0.6, 72, 360 },
  { thirty_six_minus_two_two_two_with_cam_friendly_name, thirty_six_minus_two_two_two_with_cam, 0.6, 144, 720 },
  { fourty_two_hundred_wheel_friendly_name, fourty_two_hundred_wheel, 0.6, 72, 360 },
  { thirty_six_minus_one_with_cam_fe3_friendly_name, thirty_six_minus_one_with_cam_fe3, 0.6, 144, 720 },
  { six_g_seventy_two_with_cam_friendly_name, six_g_seventy_two_with_cam, 0.6, 144, 720 },
  { buell_oddfire_cam_friendly_name, buell_oddfire_cam, 0.33333, 80, 720 },
  { gm_ls1_crank_and_cam_friendly_name, gm_ls1_crank_and_cam, 6.0, 720, 720 },
  { gm_ls_58X_crank_and_4x_cam_friendly_name, GM_LS_58X_crank_and_4x_cam, 1.0, 240, 720},
  { lotus_thirty_six_minus_one_one_one_one_friendly_name, lotus_thirty_six_minus_one_one_one_one, 0.6, 72, 360 },
  { honda_rc51_with_cam_friendly_name, honda_rc51_with_cam, 0.2, 48, 720 },
  { thirty_six_minus_one_with_second_trigger_friendly_name, thirty_six_minus_one_with_second_trigger, 0.6, 144, 720 },
  { chrysler_ngc_thirty_six_plus_two_minus_two_with_ngc4_cam_friendly_name, chrysler_ngc_thirty_six_plus_two_minus_two_with_ngc4_cam, 3.0, 720, 720 },
  { chrysler_ngc_thirty_six_minus_two_plus_two_with_ngc6_cam_friendly_name, chrysler_ngc_thirty_six_minus_two_plus_two_with_ngc6_cam, 3.0, 720, 720 },
  { chrysler_ngc_thirty_six_minus_two_plus_two_with_ngc8_cam_friendly_name, chrysler_ngc_thirty_six_minus_two_plus_two_with_ngc8_cam, 3.0, 720, 720 },
  { weber_iaw_with_cam_friendly_name, weber_iaw_with_cam, 1.2, 144, 720 },
  { fiat_one_point_eight_sixteen_valve_with_cam_friendly_name, fiat_one_point_eight_sixteen_valve_with_cam, 3.0, 720, 720 },
  { three_sixty_nissan_cas_friendly_name, three_sixty_nissan_cas, 3.0, 720, 720 },
  { twenty_four_minus_two_with_second_trigger_friendly_name, twenty_four_minus_two_with_second_trigger, 0.3, 72, 720 },
  { yamaha_eight_tooth_with_cam_friendly_name, yamaha_eight_tooth_with_cam, 0.26667, 64, 720 },
  { gm_four_tooth_with_cam_friendly_name, gm_four_tooth_with_cam, 0.06666, 8, 720 },
  { gm_six_tooth_with_cam_friendly_name, gm_six_tooth_with_cam, 0.1, 12, 720 },
  { gm_eight_tooth_with_cam_friendly_name, gm_eight_tooth_with_cam, 0.13333, 16, 720 },
  { volvo_d12acd_with_cam_friendly_name, volvo_d12acd_with_cam, 4.0, 480, 720 },
  { mazda_thirty_six_minus_two_two_two_with_six_tooth_cam_friendly_name, mazda_thirty_six_minus_two_two_two_with_six_tooth_cam, 1.5, 360, 720 },
  { mitsubishi_4g63_4_2_friendly_name, mitsubishi_4g63_4_2, 0.6, 144, 720 },
  { audi_135_with_cam_friendly_name, audi_135_with_cam, 1.5, 1080, 720 },
  { honda_d17_no_cam_friendly_name, honda_d17_no_cam, 0.6, 144, 720 },
  { mazda_323_au_friendly_name, mazda_323_au, 1, 30, 720 },
  { daihatsu_3cyl_friendly_name, daihatsu_3cyl, 0.8, 144, 360 },
  { miata_9905_friendly_name, miata_9905, 0.6, 144, 720 },
  { twelve_with_cam_friendly_name, twelve_with_cam, 0.6, 144, 720 },
  { twenty_four_with_cam_friendly_name, twenty_four_with_cam, 0.6, 144, 720 },
  { subaru_six_seven_name_friendly_name, subaru_six_seven, 3.0, 720, 720 },
  { gm_seven_x_friendly_name, gm_seven_x, 1.502, 180, 720 },
  { four_twenty_a_friendly_name, four_twenty_a, 0.6, 144, 720 },
  { ford_st170_friendly_name, ford_st170, 3.0, 720, 720 },
  { mitsubishi_3A92_friendly_name, mitsubishi_3A92, 0.6, 144, 720 },
  { Toyota_4AGE_CAS_friendly_name, toyota_4AGE_CAS, 0.333, 144, 720 },
  { Toyota_4AGZE_friendly_name, toyota_4AGZE, 0.333, 144, 720 },
  { Suzuki_DRZ400_friendly_name, suzuki_DRZ400, 0.6, 72, 360},
  { Jeep_2000_4cyl_friendly_name, jeep_2000_4cyl, 1.5, 360, 720},
  { Jeep_2000_6cyl_friendly_name, jeep_2000_6cyl, 1.5, 360, 720 },
  { BMW_N20_friendly_name, bmw_n20, 1.0, 240, 720},
  { VIPER9602_friendly_name, viper9602wheel, 1.0, 240, 720},
  { thirty_six_minus_two_with_second_trigger_friendly_name, thirty_six_minus_two_with_second_trigger, 0.6, 144, 720 },
  { GM_40_Tooth_Trans_OSS_friendly_name, GM40toothOSS, 1.0, 80, 360 },
};

// --- VARIABILI DI STATO MOTORE ---
// enum MotorState definito in globals.h
MotorState engineState = ENGINE_OFF;

float currentRpmFloat = 0.0;
float tpsValue = 0.0; // Apertura TPS normalizzata 0.0 - 1.0

// Variabili per CLT e MAP
float engineTemp = 20.0; // CLT di partenza (Temp ambiente)
float targetTemp = 90.0; // Temperatura a regime
float currentMapKpa = 100.0; // Pressione collettore (kPa)
bool fanActive = false;

// --- TIMING INIETTORI / CANDELE + UTILITA' CONNESSA (definizioni globali) ---
injectorTiming injectors[4];
ignitionTiming ignitions[4];
uint8_t connectedClients = 0;

// --- TIMER HARDWARE PER GENERAZIONE RUOTA FONICA (ESP32 v3.x API) ---
hw_timer_t * crankTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint16_t edge_counter = 0;
volatile uint8_t output_invert_mask = 0x00;
volatile uint32_t timer_period_us = 10000; // Periodo in microsecondi
volatile bool update_timer_flag = false;

// ISR del Timer Hardware
void IRAM_ATTR onCrankTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  
  if (currentStatus.rpm >= 10 && engineState != ENGINE_OFF) {
    // Registra l'istante del fronte Crank per il calcolo dell'anticipo candela
    lastCrankEdgeUs = micros();
    // Inizio del pattern (primo edge del giro): riferimento angolare del ciclo
    if (edge_counter == 0) {
      revStartUs = lastCrankEdgeUs;
    }

    // Ottiene lo stato dell'edge corrente (combinazione di bit per Crank, Cam1, Cam2)
    uint8_t edgeState = pgm_read_byte(&Wheels[config.wheel].edge_states_ptr[edge_counter]);
    edgeState ^= output_invert_mask;
    
    if (edgeState & 0x01) { GPIO.out_w1ts = (1ULL << PIN_CRANK_OUT); } else { GPIO.out_w1tc = (1ULL << PIN_CRANK_OUT); }
    if (edgeState & 0x02) { GPIO.out_w1ts = (1ULL << PIN_CAM1_OUT); } else { GPIO.out_w1tc = (1ULL << PIN_CAM1_OUT); }
    if (edgeState & 0x04) { GPIO.out_w1ts = (1ULL << PIN_CAM2_OUT); } else { GPIO.out_w1tc = (1ULL << PIN_CAM2_OUT); }
    
    edge_counter++;
    if (edge_counter >= Wheels[config.wheel].wheel_max_edges) {
      edge_counter = 0;
    }
  } else {
    // A motore spento forziamo le uscite a LOW
    GPIO.out_w1tc = (1ULL << PIN_CRANK_OUT);
    GPIO.out_w1tc = (1ULL << PIN_CAM1_OUT);
    GPIO.out_w1tc = (1ULL << PIN_CAM2_OUT);
  }

  if (update_timer_flag) {
    timerAlarm(crankTimer, timer_period_us, true, 0);
    update_timer_flag = false;
  }
  
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Calcolo del valore del timer in base agli RPM
void reset_new_OCR1A(uint32_t new_rpm) {
  if (new_rpm < 10) {
    new_rpm = 10;
  }
  
  // Il timer ESP32 è configurato a 1MHz (1 tick = 1us)
  // Calcolo del periodo tra gli edge in microsecondi
  float edges = (float)Wheels[config.wheel].wheel_max_edges;
  float divisor = (Wheels[config.wheel].wheel_degrees == 720) ? 0.5 : 1.0;
  float uSecs = (60.0 * 1000000.0) / ((float)new_rpm * edges * divisor);
  
  portENTER_CRITICAL(&timerMux);
  timer_period_us = (uint32_t)uSecs;
  if (timer_period_us < 5) timer_period_us = 5; // Limite di sicurezza
  update_timer_flag = true;
  portEXIT_CRITICAL(&timerMux);
}

void setRPM(uint16_t newRPM) {
  if (currentStatus.rpm != newRPM) {
    reset_new_OCR1A(newRPM);
  }
  currentStatus.rpm = newRPM;
}

// Configurazione Iniziale
void setup() {
  // Inizializzazione strutture
  config.version = 2;
  config.wheel = SIXTY_MINUS_TWO;
  config.mode = POT_RPM;
  config.fixed_rpm = 850;
  config.sweep_low_rpm = 250;
  config.sweep_high_rpm = 4000;
  config.sweep_interval = 1000;
  config.useCompression = false;
  config.compressionType = 0;
  config.compressionRPM = 400;
  config.compressionOffset = 0;
  config.compressionDynamic = false;

  currentStatus.base_rpm = 0;
  currentStatus.compressionModifier = 0;
  currentStatus.rpm = 0;

  serialSetup();
  
  // Configurazione I/O
  pinMode(PIN_START_STOP, INPUT_PULLUP);
  pinMode(PIN_FAN_IN, INPUT_PULLUP);
  // ADC pieno fondo scala: GPIO34 (TPS) legge il potenziometro 0-3.3V.
  // Senza atten specifico il core v3.3.x usa ~1.1V -> TPS sempre vicino a 0.
  analogSetPinAttenuation(PIN_TPS_IN, ADC_11db);
  
  pinMode(PIN_CRANK_OUT, OUTPUT);
  pinMode(PIN_CAM1_OUT, OUTPUT);
  pinMode(PIN_CAM2_OUT, OUTPUT);
  
  digitalWrite(PIN_CRANK_OUT, LOW);
  digitalWrite(PIN_CAM1_OUT, LOW);
  digitalWrite(PIN_CAM2_OUT, LOW);

  // Inizializzazione Timer (ESP32 Arduino Core v3.x API)
  // timerBegin(frequenza_Hz) - 1MHz = 1 tick per microsecondo
  crankTimer = timerBegin(1000000);
  timerAttachInterrupt(crankTimer, &onCrankTimer);
  timerAlarm(crankTimer, 10000, true, 0); // Periodo iniziale 10ms, auto-reload, infiniti cicli

  setRPM(0);

  // Configurazione monitoraggio timing iniettori/candele (FASE 2)
  timingSetup();

  // Configurazione WiFi AP + dashboard web (FASE 3)
  webSetup();
}

// Loop Principale
void loop() {
  static uint32_t lastTick = 0;
  uint32_t now = millis();
  
  // Gestione Seriale ArduStim
  if (Serial.available() > 0) {
    commandParser();
  }

  // Aggiorna contatore client WiFi connessi (asincrono, non blocca)
  webLoop();
  
  // Esecuzione logica di simulazione ogni 10ms
  if (now - lastTick >= 10) {
    uint32_t dt = now - lastTick;
    lastTick = now;
    
    // 1. Gestione Pulsante Start/Stop
    static bool lastButtonState = HIGH;
    bool btnState = digitalRead(PIN_START_STOP);
    if (btnState == LOW && lastButtonState == HIGH) {
      // Pressione rilevata
      if (engineState == ENGINE_OFF) {
        engineState = ENGINE_CRANKING;
        currentRpmFloat = 0.0;
      } else if (engineState == ENGINE_RUNNING || engineState == ENGINE_CRANKING) {
        engineState = ENGINE_STOPPING;
      }
      delay(200); // Debounce
    }
    lastButtonState = btnState;

    // 2. Lettura TPS (l'unica lettura ADC gestita dall'ESP32)
    //    IAT e O2 sono gestiti manualmente via potenziometri direttamente a Speeduino
    int adcTps = analogRead(PIN_TPS_IN);
    tpsValue = (float)adcTps / 4095.0; // Normalizzato 0.0 a 1.0

    fanActive = (digitalRead(PIN_FAN_IN) == LOW); // Attiva a livello basso

    // 3. Modello di Inerzia Giri Motore (RPM)
    float targetRpm = 0.0;
    
    switch (engineState) {
      case ENGINE_OFF:
        targetRpm = 0;
        currentRpmFloat = 0;
        break;
        
      case ENGINE_CRANKING:
        // Cranking accelera da 0 a 350 RPM
        if (currentRpmFloat < 350.0) {
          currentRpmFloat += 2.0; // Rampa di cranking
        } else {
          // Motore si avvia
          engineState = ENGINE_RUNNING;
        }
        break;
        
      case ENGINE_RUNNING:
        // RPM minimo in idle ~850. Massimo ~7000 basato su TPS
        targetRpm = 850.0 + (tpsValue * 6150.0);
        // Simulazione inerzia (Filtro passa-basso su accelerazione)
        currentRpmFloat += (targetRpm - currentRpmFloat) * 0.03; 
        break;
        
      case ENGINE_STOPPING:
        // Decelerazione fino all'arresto
        currentRpmFloat -= 15.0;
        if (currentRpmFloat <= 0.0) {
          currentRpmFloat = 0.0;
          engineState = ENGINE_OFF;
        }
        break;
    }
    
    currentStatus.base_rpm = (uint16_t)currentRpmFloat;
    setRPM(currentStatus.base_rpm);

    // 4. Simulazione MAP (Pressione collettore)
    currentMapKpa = 100.0; // Pressione atmosferica di default (motore spento)
    if (engineState == ENGINE_RUNNING) {
      float idleMap = 30.0;
      float decelMap = 15.0;
      
      if (currentRpmFloat > 1000.0 && tpsValue < 0.05) {
        // Cut-off o decelerazione a farfalla chiusa
        currentMapKpa = decelMap;
      } else {
        // Interpolazione tra vuoto al minimo e carico massimo
        currentMapKpa = idleMap + (tpsValue * 70.0);
      }
    } else if (engineState == ENGINE_CRANKING) {
      currentMapKpa = 85.0; // Lieve depressione durante il cranking
    } else {
      currentMapKpa = 100.0; // Motore spento -> Atmosferica
    }
    
    // Converte MAP (kPa 0-100) in tensione analogica (DAC 8 bit: 0-255 -> 0-3.3V)
    uint8_t dacMapVal = (uint8_t)(currentMapKpa * 2.55);
    dacWrite(PIN_MAP_DAC, dacMapVal);

    // 5. Simulazione CLT (Temperatura Motore)
    if (engineState == ENGINE_RUNNING || engineState == ENGINE_CRANKING) {
      // Il motore si scalda. La velocità dipende dagli RPM
      float heatingRate = 0.002 + (currentRpmFloat / 7000.0) * 0.01;
      if (fanActive) {
        // La ventola di raffreddamento contrasta il riscaldamento
        heatingRate -= 0.008;
      }
      engineTemp += heatingRate;
      if (engineTemp > 105.0) engineTemp = 105.0; // Limite termostatico massimo
    } else {
      // Motore spento: raffreddamento naturale (Newton)
      float ambientTemp = 20.0;
      engineTemp += (ambientTemp - engineTemp) * 0.0001;
    }
    
    // Mappatura temperatura 20-120°C su DAC (inversa, NTC-style)
    float tempPercent = (engineTemp - 20.0) / 100.0;
    if (tempPercent < 0.0) tempPercent = 0.0;
    if (tempPercent > 1.0) tempPercent = 1.0;
    
    uint8_t dacCltVal = (uint8_t)((1.0 - tempPercent) * 255.0);
    dacWrite(PIN_CLT_DAC, dacCltVal);

    // Calcolo timing iniettori/candele dai timestamp grezzi (FASE 2)
    timingUpdate();
  }
}
