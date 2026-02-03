// storage.h
#pragma once
#include <Arduino.h>
#include <WebServer.h>

// Deklaracje funkcji do odczytu danych
const char* storage_get_profile_path();
const char* storage_get_wifi_ssid();
const char* storage_get_wifi_pass();

// Deklaracje funkcji do modyfikacji danych
bool storage_load_profile();
void storage_load_config_nvs();
void storage_save_wifi_nvs(const char* ssid, const char* pass);
void storage_save_profile_path_nvs(const char* path);
void storage_save_manual_settings_nvs();

// Funkcje zwracające JSON przez ArduinoJson - serializują bezpośrednio do WebServer
void storage_list_profiles_json(WebServer& server);
void storage_get_profile_as_json(WebServer& server, const char* profileName);
void storage_list_github_profiles_json(WebServer& server);

bool storage_reinit_sd();
bool storage_load_github_profile(const char* profileName);