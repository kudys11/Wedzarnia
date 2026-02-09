// webhooks.h - Zaktualizowany z nowymi funkcjami
#pragma once
#include <Arduino.h>

// ======================================================
// KONFIGURACJA HOME ASSISTANT
// ======================================================

// Odkomentuj aby włączyć integrację z HA
// #define ENABLE_HOME_ASSISTANT

#ifdef ENABLE_HOME_ASSISTANT

// 1. Adres Twojego Home Assistant
const char* HA_SERVER = "https://hassio.pikur.eu/";

// 2. Long-Lived Access Token z HA
const char* HA_TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJhNDFhNzJlOGJjM2I0OWFhODJhMWNiNTcwODk1NzYyYiIsImlhdCI6MTc3MDM4MjQ1MSwiZXhwIjoyMDg1NzQyNDUxfQ.Knd5oRa9Q-BCOxLbfEBj659TV3Jkwly92fQvhRQkiN8";

// 3. Interwał wysyłania danych (milisekundy)
const unsigned long HA_UPDATE_INTERVAL = 30000; // 30 sekund

// 4. Entity IDs w Home Assistant
const char* HA_ENTITY_TEMP = "sensor.wedzarnia_temperature";
const char* HA_ENTITY_STATE = "sensor.wedzarnia_state";
const char* HA_ENTITY_MEAT = "sensor.wedzarnia_meat_temp";

// 5. Webhook ID dla automatyzacji (opcjonalnie)
const char* HA_WEBHOOK_ID = "wedzarnia_notification";

// ======================================================
// FUNKCJE PUBLICZNE
// ======================================================

// Inicjalizacja
void ha_webhook_init();

// Główna pętla (wywoływać regularnie)
void ha_webhook_loop();

// Wysyłanie danych
void ha_send_temperature(float chamber, float meat, float setpoint);
void ha_send_state(const char* state, const char* icon = nullptr);
void ha_send_alert(const char* title, const char* message);
void ha_send_statistics();
void ha_send_all_data(); // Wyślij wszystkie dane na raz

// Webhook i automatyzacje
void ha_trigger_webhook(const char* webhook_id, const char* data = "");

// Połączenie i status
bool ha_test_connection();
bool ha_is_connected();
void ha_reset_connection();

// Rejestracja urządzenia
void ha_register_device();

// ======================================================
// FUNKCJE POMOCNICZE (DODANE)
// ======================================================

// Pomocnicza funkcja do formatowania czasu
static String format_time(unsigned long seconds);

// Funkcja do tworzenia atrybutów JSON
static String create_ha_attributes(const char* friendly_name, const char* icon, 
                                   const JsonObject& extra_attrs = JsonObject());

// Funkcja do bezpiecznego wysyłania HTTP
static bool send_ha_request(const char* endpoint, const char* method, 
                           const String& payload, const char* operation);

#else
// Puste implementacje gdy HA wyłączony
inline void ha_webhook_init() { 
    Serial.println("Home Assistant integration disabled"); 
}
inline void ha_webhook_loop() {}
inline void ha_send_temperature(float, float, float) {}
inline void ha_send_state(const char*, const char* = nullptr) {}
inline void ha_send_alert(const char*, const char*) {}
inline void ha_send_statistics() {}
inline void ha_send_all_data() {}
inline void ha_trigger_webhook(const char*, const char* = "") {}
inline bool ha_test_connection() { return false; }
inline bool ha_is_connected() { return false; }
inline void ha_reset_connection() {}
inline void ha_register_device() {}
#endif