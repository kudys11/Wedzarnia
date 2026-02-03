// storage.h
#pragma once
#include <stddef.h>

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

// Profile listing functions - now use static buffers
// Writes JSON string to buffer, returns actual length written (not including null terminator)
// Returns 0 on error
size_t storage_list_profiles_json(char* buffer, size_t buffer_size);
size_t storage_list_github_profiles_json(char* buffer, size_t buffer_size);
size_t storage_get_profile_as_json(const char* profileName, char* buffer, size_t buffer_size);

bool storage_reinit_sd();

// GITHUB profile loading
bool storage_load_github_profile(const char* profileName);