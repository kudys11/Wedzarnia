// bluetooth.h - BLE z NimBLE (ESP32-S3)
// Wymagana biblioteka: NimBLE-Arduino by h2zero
// Instalacja: Sketch → Include Library → Manage Libraries → "NimBLE-Arduino"
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

// ======================================================
// UUIDs serwisu i charakterystyk
// ======================================================
#define BLE_SERVICE_UUID        "12345678-1234-1234-1234-123456789000"
#define BLE_CHAR_STATUS_UUID    "12345678-1234-1234-1234-123456789001" // notify → telefon (auto-status)
#define BLE_CHAR_COMMAND_UUID   "12345678-1234-1234-1234-123456789002" // write  ← telefon (komendy)
#define BLE_CHAR_RESPONSE_UUID  "12345678-1234-1234-1234-123456789003" // notify → telefon (odpowiedzi)
#define BLE_DEVICE_NAME         "Wedzarnia_S3"

// ======================================================
// Publiczne API
// ======================================================
void ble_init();
void ble_handle();           // Wywołuj w loop() - obsługa kolejki komend
void ble_send_status();      // Wyślij pełny status do telefonu
void ble_notify(const String& message); // Wyślij odpowiedź/log
bool ble_is_connected();

// ======================================================
// Komendy - identyczne jak poprzednio
// Aplikacja Android nie wymaga zmian!
// ======================================================
namespace BTCommand {
    constexpr const char* PING         = "PING";
    constexpr const char* GET_STATUS   = "GET_STATUS";
    constexpr const char* GET_TEMPS    = "GET_TEMPS";
    constexpr const char* GET_PROFILES = "GET_PROFILES";
    constexpr const char* START_AUTO   = "START_AUTO";
    constexpr const char* START_MANUAL = "START_MANUAL";
    constexpr const char* STOP         = "STOP";
    constexpr const char* PAUSE        = "PAUSE";
    constexpr const char* RESUME       = "RESUME";
    constexpr const char* SET_TEMP     = "SET_TEMP";      // SET_TEMP:75.5
    constexpr const char* SET_POWER    = "SET_POWER";     // SET_POWER:2
    constexpr const char* SET_SMOKE    = "SET_SMOKE";     // SET_SMOKE:128
    constexpr const char* SET_FAN      = "SET_FAN";       // SET_FAN:1
    constexpr const char* LOAD_PROFILE = "LOAD_PROFILE";  // LOAD_PROFILE:test.prof
    constexpr const char* NEXT_STEP    = "NEXT_STEP";
}
