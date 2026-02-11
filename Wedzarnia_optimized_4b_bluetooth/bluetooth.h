// bluetooth.h - Obsługa Bluetooth Classic dla wędzarni
#pragma once
#include <Arduino.h>
#include <BluetoothSerial.h>

// Sprawdź czy BT jest dostępne
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// Inicjalizacja Bluetooth
void bluetooth_init();

// Obsługa komunikacji Bluetooth
void bluetooth_handle_communication();

// Wysyłanie danych do aplikacji
void bluetooth_send_status();
void bluetooth_send_temperatures();
void bluetooth_send_profile_list();

// Parsowanie komend z aplikacji
void bluetooth_parse_command(String command);

// Sprawdź czy klient jest podłączony
bool bluetooth_is_connected();

// Wyślij log do aplikacji
void bluetooth_log(const String& message);

// Nazwy komend
namespace BTCommand {
    // Status i odczyty
    constexpr const char* GET_STATUS = "GET_STATUS";
    constexpr const char* GET_TEMPS = "GET_TEMPS";
    constexpr const char* GET_PROFILES = "GET_PROFILES";
    
    // Sterowanie procesem
    constexpr const char* START_AUTO = "START_AUTO";
    constexpr const char* START_MANUAL = "START_MANUAL";
    constexpr const char* STOP = "STOP";
    constexpr const char* PAUSE = "PAUSE";
    constexpr const char* RESUME = "RESUME";
    
    // Ustawienia manualne
    constexpr const char* SET_TEMP = "SET_TEMP";        // SET_TEMP:75.5
    constexpr const char* SET_POWER = "SET_POWER";      // SET_POWER:2
    constexpr const char* SET_SMOKE = "SET_SMOKE";      // SET_SMOKE:128
    constexpr const char* SET_FAN = "SET_FAN";          // SET_FAN:1
    
    // Profil
    constexpr const char* LOAD_PROFILE = "LOAD_PROFILE"; // LOAD_PROFILE:test.prof
    constexpr const char* NEXT_STEP = "NEXT_STEP";
    
    // Ping
    constexpr const char* PING = "PING";
}

// Nazwy odpowiedzi
namespace BTResponse {
    constexpr const char* OK = "OK";
    constexpr const char* ERROR = "ERROR";
    constexpr const char* PONG = "PONG";
}
