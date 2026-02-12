// storage.cpp - Zmodernizowana wersja z backup
#include "storage.h"
#include "config.h"
#include "state.h"
#include <SD.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "FS.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Zmienne konfiguracyjne
static char lastProfilePath[64] = "/profiles/test.prof";
static char wifiStaSsid[32] = "";
static char wifiStaPass[64] = "";

// Backup counter
static int backupCounter = 0;
static constexpr int MAX_BACKUPS = 5;

// Funkcja pomocnicza
static bool parseBool(const char* s) {
    return (strcmp(s, "1") == 0 || strcasecmp(s, "true") == 0);
}

const char* storage_get_profile_path() { return lastProfilePath; }
const char* storage_get_wifi_ssid() { return wifiStaSsid; }
const char* storage_get_wifi_pass() { return wifiStaPass; }

// Funkcja pomocnicza do parsowania kroków profilu
static bool parseProfileLine(char* line, Step& step) {
    // Usuń białe znaki z początku
    while (*line == ' ' || *line == '\t') line++;
    
    // Usuń znaki końca linii
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) {
        line[--len] = '\0';
    }
    
    // Pomiń puste linie i komentarze
    if (len == 0 || line[0] == '#') return false;
    
    // Parsuj pola
    char* fields[10];
    int fieldCount = 0;
    char* token = strtok(line, ";");
    while (token && fieldCount < 10) {
        fields[fieldCount++] = token;
        token = strtok(NULL, ";");
    }
    
    if (fieldCount < 10) {
        log_msg(LOG_LEVEL_WARN, "Invalid profile line - not enough fields");
        return false;
    }
    
    // Wypełnij strukturę Step
    strncpy(step.name, fields[0], sizeof(step.name) - 1);
    step.name[sizeof(step.name) - 1] = '\0';
    step.tSet = constrain(atof(fields[1]), CFG_T_MIN_SET, CFG_T_MAX_SET);
    step.tMeatTarget = constrain(atof(fields[2]), 0, 100);
    step.minTimeMs = (unsigned long)(atoi(fields[3])) * 60UL * 1000UL;
    step.powerMode = constrain(atoi(fields[4]), CFG_POWERMODE_MIN, CFG_POWERMODE_MAX);
    step.smokePwm = constrain(atoi(fields[5]), CFG_SMOKE_PWM_MIN, CFG_SMOKE_PWM_MAX);
    step.fanMode = constrain(atoi(fields[6]), 0, 2);
    step.fanOnTime = max(1000UL, (unsigned long)(atoi(fields[7])) * 1000UL);
    step.fanOffTime = max(1000UL, (unsigned long)(atoi(fields[8])) * 1000UL);
    step.useMeatTemp = parseBool(fields[9]);
    
    return true;
}

bool storage_load_profile() {
    String pathStr = String(lastProfilePath);

    if (pathStr.startsWith("github:")) {
        String profileName = pathStr.substring(7);
        return storage_load_github_profile(profileName.c_str());
    } else {
        if (!SD.exists(lastProfilePath)) {
            log_msg(LOG_LEVEL_ERROR, "Profile not found on SD: " + String(lastProfilePath));
            if (state_lock()) {
                g_errorProfile = true;
                state_unlock();
            }
            return false;
        }
        
        // Utwórz backup przed załadowaniem
        storage_backup_config();
        
        File f = SD.open(lastProfilePath, "r");
        if (!f) {
            log_msg(LOG_LEVEL_ERROR, "Cannot open profile file");
            if (state_lock()) {
                g_errorProfile = true;
                state_unlock();
            }
            return false;
        }
        
        int loadedStepCount = 0;
        char lineBuf[256];
        
        while (f.available() && loadedStepCount < MAX_STEPS) {
            int len = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
            lineBuf[len] = '\0';
            
            if (parseProfileLine(lineBuf, g_profile[loadedStepCount])) {
                loadedStepCount++;
            }
        }
        f.close();
        
        if (state_lock()) {
            g_stepCount = loadedStepCount;
            g_errorProfile = (g_stepCount == 0);
            
            // Oblicz całkowity czas procesu
            g_processStats.totalProcessTimeSec = 0;
            for (int i = 0; i < g_stepCount; i++) {
                g_processStats.totalProcessTimeSec += g_profile[i].minTimeMs / 1000;
            }
            
            state_unlock();
        }
        
        if (g_errorProfile) {
            log_msg(LOG_LEVEL_ERROR, "Failed to load profile: " + String(lastProfilePath));
        } else {
            log_msg(LOG_LEVEL_INFO, "Profile loaded from SD: " + String(g_stepCount) + " steps");
        }
        
        return !g_errorProfile;
    }
}

void storage_load_config_nvs() {
    nvs_handle_t nvsHandle;
    if (nvs_open("wedzarnia", NVS_READONLY, &nvsHandle) != ESP_OK) {
        log_msg(LOG_LEVEL_INFO, "No saved config in NVS");
        return;
    }
    
    size_t len;
    len = sizeof(wifiStaSsid);
    if (nvs_get_str(nvsHandle, "wifi_ssid", wifiStaSsid, &len) != ESP_OK) 
        wifiStaSsid[0] = '\0';
    
    len = sizeof(wifiStaPass);
    if (nvs_get_str(nvsHandle, "wifi_pass", wifiStaPass, &len) != ESP_OK) 
        wifiStaPass[0] = '\0';
    
    len = sizeof(lastProfilePath);
    if (nvs_get_str(nvsHandle, "profile", lastProfilePath, &len) != ESP_OK) {
        strcpy(lastProfilePath, "/profiles/test.prof");
    }
    
    if (state_lock()) {
        double tmp_d;
        len = sizeof(tmp_d);
        if (nvs_get_blob(nvsHandle, "manual_tset", &tmp_d, &len) == ESP_OK) 
            g_tSet = tmp_d;
        
        int32_t tmp_i;
        if (nvs_get_i32(nvsHandle, "manual_pow", &tmp_i) == ESP_OK) 
            g_powerMode = tmp_i;
        
        if (nvs_get_i32(nvsHandle, "manual_smoke", &tmp_i) == ESP_OK) 
            g_manualSmokePwm = tmp_i;
        
        if (nvs_get_i32(nvsHandle, "manual_fan", &tmp_i) == ESP_OK) 
            g_fanMode = tmp_i;
        
        state_unlock();
    }
    
    nvs_close(nvsHandle);
    log_msg(LOG_LEVEL_INFO, "NVS config loaded");
}

static void nvs_save_generic(std::function<void(nvs_handle_t)> action) {
    nvs_handle_t nvsHandle;
    if (nvs_open("wedzarnia", NVS_READWRITE, &nvsHandle) == ESP_OK) {
        action(nvsHandle);
        nvs_commit(nvsHandle);
        nvs_close(nvsHandle);
    }
}

void storage_save_wifi_nvs(const char* ssid, const char* pass) {
    strncpy(wifiStaSsid, ssid, sizeof(wifiStaSsid) - 1);
    wifiStaSsid[sizeof(wifiStaSsid) - 1] = '\0';
    strncpy(wifiStaPass, pass, sizeof(wifiStaPass) - 1);
    wifiStaPass[sizeof(wifiStaPass) - 1] = '\0';
    
    nvs_save_generic([&](nvs_handle_t handle){
        nvs_set_str(handle, "wifi_ssid", wifiStaSsid);
        nvs_set_str(handle, "wifi_pass", wifiStaPass);
    });
    
    log_msg(LOG_LEVEL_INFO, "WiFi credentials saved to NVS");
}

void storage_save_profile_path_nvs(const char* path) {
    strncpy(lastProfilePath, path, sizeof(lastProfilePath) - 1);
    lastProfilePath[sizeof(lastProfilePath) - 1] = '\0';
    
    nvs_save_generic([&](nvs_handle_t handle){
        nvs_set_str(handle, "profile", lastProfilePath);
    });
    
    log_msg(LOG_LEVEL_INFO, "Profile path saved: " + String(path));
}

void storage_save_manual_settings_nvs() {
    if (!state_lock()) return;
    
    double ts = g_tSet;
    int pm = g_powerMode;
    int sm = g_manualSmokePwm;
    int fm = g_fanMode;
    state_unlock();
    
    nvs_save_generic([=](nvs_handle_t handle){
        nvs_set_blob(handle, "manual_tset", &ts, sizeof(ts));
        nvs_set_i32(handle, "manual_pow", pm);
        nvs_set_i32(handle, "manual_smoke", sm);
        nvs_set_i32(handle, "manual_fan", fm);
    });
    
    log_msg(LOG_LEVEL_DEBUG, "Manual settings saved to NVS");
}

String storage_list_profiles_json() {
    String json = "[";
    bool first = true;
    
    File root = SD.open("/profiles");
    if (!root || !root.isDirectory()) {
        log_msg(LOG_LEVEL_WARN, "Cannot open /profiles directory");
        return "[]";
    }
    
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fileName = file.name();
            if (fileName.endsWith(".prof")) {
                if (!first) { json += ","; }
                json += "\"" + fileName + "\"";
                first = false;
            }
        }
        file = root.openNextFile();
    }
    root.close();
    json += "]";
    
    return json;
}

bool storage_reinit_sd() {
    log_msg(LOG_LEVEL_INFO, "Re-initializing SD card...");
    SD.end();
    delay(200);
    
    if (SD.begin(PIN_SD_CS)) {
        log_msg(LOG_LEVEL_INFO, "SD card re-initialized successfully");
        return true;
    } else {
        log_msg(LOG_LEVEL_ERROR, "Failed to re-initialize SD card");
        return false;
    }
}

String storage_get_profile_as_json(const char* profileName) {
    String path = "/profiles/" + String(profileName);
    if (!SD.exists(path)) {
        log_msg(LOG_LEVEL_WARN, "Profile not found: " + path);
        return "[]";
    }
    
    File f = SD.open(path, "r");
    if (!f) {
        log_msg(LOG_LEVEL_ERROR, "Cannot open profile: " + path);
        return "[]";
    }
    
    String json = "[";
    bool firstStep = true;
    char lineBuf[256];
    
    while (f.available()) {
        int len = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[len] = '\0';
        
        char* line = lineBuf;
        while (*line == ' ' || *line == '\t') line++;
        len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) {
            line[--len] = '\0';
        }
        if (len == 0 || line[0] == '#') continue;
        
        char* fields[10];
        int fieldCount = 0;
        char* token = strtok(line, ";");
        while (token && fieldCount < 10) {
            fields[fieldCount++] = token;
            token = strtok(NULL, ";");
        }
        if (fieldCount < 10) continue;
        
        if (!firstStep) { json += ","; }
        json += "{";
        json += "\"name\":\"" + String(fields[0]) + "\",";
        json += "\"tSet\":" + String(fields[1]) + ",";
        json += "\"tMeat\":" + String(fields[2]) + ",";
        json += "\"minTime\":" + String(fields[3]) + ",";
        json += "\"powerMode\":" + String(fields[4]) + ",";
        json += "\"smoke\":" + String(fields[5]) + ",";
        json += "\"fanMode\":" + String(fields[6]) + ",";
        json += "\"fanOn\":" + String(fields[7]) + ",";
        json += "\"fanOff\":" + String(fields[8]) + ",";
        json += "\"useMeatTemp\":" + String(fields[9]);
        json += "}";
        firstStep = false;
    }
    f.close();
    json += "]";
    
    return json;
}

String storage_list_github_profiles_json() {
    if (WiFi.status() != WL_CONNECTED) {
        log_msg(LOG_LEVEL_WARN, "WiFi not connected - cannot list GitHub profiles");
        return "[\"Brak WiFi\"]";
    }
    
    HTTPClient http;
    http.begin(CFG_GITHUB_API_URL);
    http.addHeader("User-Agent", "ESP32-Wedzarnia-Client");
    http.setTimeout(5000);
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        log_msg(LOG_LEVEL_ERROR, "GitHub API error: " + String(httpCode));
        http.end();
        return "[\"Blad API GitHub\"]";
    }
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    http.end();
    
    if (error) {
        log_msg(LOG_LEVEL_ERROR, "JSON parse error: " + String(error.c_str()));
        return "[\"Blad parsowania\"]";
    }
    
    String json = "[";
    bool first = true;
    for (JsonVariant value : doc.as<JsonArray>()) {
        const char* filename = value["name"];
        String filenameStr = String(filename);
        if (filenameStr.endsWith(".prof")) {
            if (!first) { json += ","; }
            json += "\"" + filenameStr + "\"";
            first = false;
        }
    }
    json += "]";
    
    return json;
}

bool storage_load_github_profile(const char* profileName) {
    if (WiFi.status() != WL_CONNECTED) {
        log_msg(LOG_LEVEL_ERROR, "WiFi not connected - cannot load from GitHub");
        if (state_lock()) {
            g_errorProfile = true;
            state_unlock();
        }
        return false;
    }
    
    HTTPClient http;
    String url = String(CFG_GITHUB_PROFILES_BASE_URL) + String(profileName);
    log_msg(LOG_LEVEL_INFO, "Loading profile from: " + url);
    
    http.begin(url);
    http.setTimeout(10000);
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        log_msg(LOG_LEVEL_ERROR, "GitHub download error: " + String(httpCode));
        http.end();
        if (state_lock()) {
            g_errorProfile = true;
            state_unlock();
        }
        return false;
    }
    
    WiFiClient* stream = http.getStreamPtr();
    int loadedStepCount = 0;
    char lineBuf[256];
    
    while (http.connected() && stream->available() && loadedStepCount < MAX_STEPS) {
        int len = stream->readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[len] = '\0';
        
        if (parseProfileLine(lineBuf, g_profile[loadedStepCount])) {
            loadedStepCount++;
        }
    }
    http.end();
    
    if (state_lock()) {
        g_stepCount = loadedStepCount;
        g_errorProfile = (g_stepCount == 0);
        
        // Oblicz całkowity czas procesu
        g_processStats.totalProcessTimeSec = 0;
        for (int i = 0; i < g_stepCount; i++) {
            g_processStats.totalProcessTimeSec += g_profile[i].minTimeMs / 1000;
        }
        
        state_unlock();
    }
    
    if (g_errorProfile) {
        log_msg(LOG_LEVEL_ERROR, "Failed to load profile from GitHub: " + String(profileName));
    } else {
        log_msg(LOG_LEVEL_INFO, "Profile '" + String(profileName) + "' loaded from GitHub: " + 
                String(g_stepCount) + " steps");
    }
    
    return !g_errorProfile;
}

// Nowe funkcje: backup konfiguracji

void storage_backup_config() {
    backupCounter++;
    if (backupCounter % 5 != 0) return; // Backup co 5 załadowań profilu
    
    String backupPath = "/backup/config_" + String(millis() / 1000) + ".bak";
    
    // Utwórz katalog backup jeśli nie istnieje
    if (!SD.exists("/backup")) {
        if (!SD.mkdir("/backup")) {
            log_msg(LOG_LEVEL_ERROR, "Failed to create backup directory");
            return;
        }
    }
    
    File backupFile = SD.open(backupPath.c_str(), FILE_WRITE);
    if (!backupFile) {
        log_msg(LOG_LEVEL_ERROR, "Failed to create backup file");
        return;
    }
    
    StaticJsonDocument<512> doc;
    doc["profile_path"] = lastProfilePath;
    doc["wifi_ssid"] = wifiStaSsid;
    doc["backup_timestamp"] = millis() / 1000;
    
    serializeJson(doc, backupFile);
    backupFile.close();
    
    log_msg(LOG_LEVEL_INFO, "Config backup created: " + backupPath);
    
    // Usuń najstarsze backupy jeśli jest ich za dużo
    cleanupOldBackups();
}

void cleanupOldBackups() {
    File backupDir = SD.open("/backup");
    if (!backupDir) return;
    
    std::vector<String> backupFiles;
    
    while (File entry = backupDir.openNextFile()) {
        if (!entry.isDirectory()) {
            String fileName = entry.name();
            if (fileName.endsWith(".bak")) {
                backupFiles.push_back(fileName);
            }
        }
        entry.close();
    }
    backupDir.close();
    
    if (backupFiles.size() > MAX_BACKUPS) {
        // Sortuj po nazwie (timestamp jest w nazwie)
        std::sort(backupFiles.begin(), backupFiles.end());
        
        // Usuń najstarsze
        int filesToDelete = backupFiles.size() - MAX_BACKUPS;
        for (int i = 0; i < filesToDelete; i++) {
            if (SD.remove(backupFiles[i].c_str())) {
                log_msg(LOG_LEVEL_INFO, "Deleted old backup: " + backupFiles[i]);
            }
        }
    }
}

bool storage_restore_backup(const char* backupPath) {
    if (!SD.exists(backupPath)) {
        log_msg(LOG_LEVEL_ERROR, "Backup file not found: " + String(backupPath));
        return false;
    }
    
    File backupFile = SD.open(backupPath, "r");
    if (!backupFile) {
        log_msg(LOG_LEVEL_ERROR, "Cannot open backup file");
        return false;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, backupFile);
    backupFile.close();
    
    if (error) {
        log_msg(LOG_LEVEL_ERROR, "Failed to parse backup JSON: " + String(error.c_str()));
        return false;
    }
    
    const char* profilePath = doc["profile_path"];
    const char* wifiSsid = doc["wifi_ssid"];
    unsigned long timestamp = doc["backup_timestamp"];
    
    if (profilePath) {
        strncpy(lastProfilePath, profilePath, sizeof(lastProfilePath) - 1);
        log_msg(LOG_LEVEL_INFO, "Restored profile path: " + String(profilePath));
    }
    
    if (wifiSsid && strlen(wifiSsid) > 0) {
        strncpy(wifiStaSsid, wifiSsid, sizeof(wifiStaSsid) - 1);
        log_msg(LOG_LEVEL_INFO, "Restored WiFi SSID: " + String(wifiSsid));
    }
    
    // Zapisz do NVS
    storage_save_profile_path_nvs(lastProfilePath);
    storage_save_wifi_nvs(wifiStaSsid, wifiStaPass);
    
    log_msg(LOG_LEVEL_INFO, "Backup restored (timestamp: " + String(timestamp) + ")");
    return true;
}

String storage_list_backups_json() {
    String json = "[";
    bool first = true;
    
    if (!SD.exists("/backup")) {
        return "[]";
    }
    
    File backupDir = SD.open("/backup");
    if (!backupDir || !backupDir.isDirectory()) {
        return "[]";
    }
    
    while (File entry = backupDir.openNextFile()) {
        if (!entry.isDirectory()) {
            String fileName = entry.name();
            if (fileName.endsWith(".bak")) {
                if (!first) { json += ","; }
                json += "\"" + fileName + "\"";
                first = false;
            }
        }
        entry.close();
    }
    backupDir.close();
    json += "]";
    
    return json;
}