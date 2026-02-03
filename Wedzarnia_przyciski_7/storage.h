// storage.h
#pragma once
#include <Arduino.h>
// Deklaracje funkcji do odczytu danych
const char* storage_get_profile_path(); // <--- TA LINIĘ TEŻ WARTO DODAĆ
const char* storage_get_wifi_ssid();    // <--- DODAJ TĘ LINIĘ
const char* storage_get_wifi_pass();    // <--- DODAJ TĘ LINIĘ

// Deklaracje funkcji do modyfikacji danych
bool storage_load_profile();
void storage_load_config_nvs();
void storage_save_wifi_nvs(const char* ssid, const char* pass);
void storage_save_profile_path_nvs(const char* path);
void storage_save_manual_settings_nvs();
String storage_list_profiles_json();
bool storage_reinit_sd();
String storage_get_profile_as_json(const char* profileName);

// NOWE DEKLARACJE GITHUB
String storage_list_github_profiles_json();
bool storage_load_github_profile(const char* profileName);