/*
 * web.h - Server web WiFi (Modalita' AP) per Engine Simulator 2.0
 *
 * Fornisce:
 *   - WiFi Access Point (SSID/password/IP fissi, con DHCP per i client)
 *   - Dashboard motore su "/"
 *   - Selezione ruota fonica / salvataggio su NVS su "/setup"
 *   - Endpoint JSON "/api/data" per l'aggiornamento automatico della dashboard
 *
 * Usa ESPAsyncWebServer (asincrono, non blocca il loop di simulazione).
 */

#ifndef __WEB_H__
#define __WEB_H__

#include <Arduino.h>

// --- CONFIGURAZIONE ACCESS POINT ---
#define AP_SSID      "EngineSimulator2.0"
#define AP_PASSWORD  "1234567890"
#define AP_IP        192, 168, 254, 1
#define AP_GATEWAY   192, 168, 254, 1
#define AP_SUBNET    255, 255, 255, 0

// --- CHIAVE NVS PER LA RUOTA FONICA ---
#define NVS_NAMESPACE "engsim"
#define NVS_KEY_WHEEL "wheel"

void webSetup();
void webLoop();

#endif
