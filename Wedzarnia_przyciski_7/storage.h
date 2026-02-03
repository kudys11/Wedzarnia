// storage.h
#pragma once
#include <Arduino.h>
#include "config.h"

// Deklaracje funkcji do odczytu danych
const char* storage_get_profile_path();
const char* storage_get_wifi_ssid();
const char* storage_get_wifi_pass();

// Profile list access
extern char profileNames[MAX_PROFILES][MAX_PROFILE_NAME_LEN];
extern int profileCount;

// Deklaracje funkcji do modyfikacji danych
bool storage_load_profile();
void storage_load_config_nvs();
void storage_save_wifi_nvs(const char* ssid, const char* pass);
void storage_save_profile_path_nvs(const char* path);
void storage_save_manual_settings_nvs();
void storage_list_profiles_json(char* buffer, size_t bufferSize);
bool storage_reinit_sd();
void storage_get_profile_as_json(const char* profileName, char* buffer, size_t bufferSize);

// NOWE DEKLARACJE GITHUB
void storage_list_github_profiles_json(char* buffer, size_t bufferSize);
bool storage_load_github_profile(const char* profileName);