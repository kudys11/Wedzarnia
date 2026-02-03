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

// Zmienne konfiguracyjne - prywatne dla tego modułu
static char lastProfilePath[64] = "/profiles/test.prof";
static char wifiStaSsid[32] = "";
static char wifiStaPass[64] = "";

// Funkcja pomocnicza
static bool parseBool(const char* s) {
    return (strcmp(s, "1") == 0 || strcasecmp(s, "true") == 0);
}

const char* storage_get_profile_path() { return lastProfilePath; }
const char* storage_get_wifi_ssid() { return wifiStaSsid; }
const char* storage_get_wifi_pass() { return wifiStaPass; }

// ZMODYFIKOWANA funkcja, teraz działa jako "router"
bool storage_load_profile() {
    String pathStr = String(lastProfilePath);

    if (pathStr.startsWith("github:")) {
        String profileName = pathStr.substring(7); // Usuń prefix "github:"
        return storage_load_github_profile(profileName.c_str());
    } else {
        // Istniejąca logika dla karty SD
        if (!SD.exists(lastProfilePath)) {
            Serial.printf("Profile not found on SD: %s\n", lastProfilePath);
            state_lock(); g_errorProfile = true; state_unlock();
            return false;
        }
        File f = SD.open(lastProfilePath, "r");
        if (!f) {
            state_lock(); g_errorProfile = true; state_unlock();
            return false;
        }
        int loadedStepCount = 0;
        char lineBuf[256];
        while (f.available() && loadedStepCount < MAX_STEPS) {
            int len = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
            lineBuf[len] = '\0';
            char* line = lineBuf;
            while (*line == ' ' || *line == '\t') line++;
            len = strlen(line);
            while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) { line[--len] = '\0'; }
            if (len == 0 || line[0] == '#') continue;
            char* fields[10];
            int fieldCount = 0;
            char* token = strtok(line, ";");
            while (token && fieldCount < 10) { fields[fieldCount++] = token; token = strtok(NULL, ";"); }
            if (fieldCount < 10) continue;
            Step& s = g_profile[loadedStepCount];
            strncpy(s.name, fields[0], sizeof(s.name) - 1);
            s.name[sizeof(s.name) - 1] = '\0';
            s.tSet = constrain(atof(fields[1]), CFG_T_MIN_SET, CFG_T_MAX_SET);
            s.tMeatTarget = constrain(atof(fields[2]), 0, 100);
            s.minTimeMs = (unsigned long)(atoi(fields[3])) * 60UL * 1000UL;
            s.powerMode = constrain(atoi(fields[4]), CFG_POWERMODE_MIN, CFG_POWERMODE_MAX);
            s.smokePwm = constrain(atoi(fields[5]), CFG_SMOKE_PWM_MIN, CFG_SMOKE_PWM_MAX);
            s.fanMode = constrain(atoi(fields[6]), 0, 2);
            s.fanOnTime = max(1000UL, (unsigned long)(atoi(fields[7])) * 1000UL);
            s.fanOffTime = max(1000UL, (unsigned long)(atoi(fields[8])) * 1000UL);
            s.useMeatTemp = parseBool(fields[9]);
            loadedStepCount++;
        }
        f.close();
        state_lock();
        g_stepCount = loadedStepCount;
        g_errorProfile = (g_stepCount == 0);
        state_unlock();
        Serial.printf("Profile loaded from SD: %d steps\n", g_stepCount);
        return !g_errorProfile;
    }
}

void storage_load_config_nvs() {
    nvs_handle_t nvsHandle;
    if (nvs_open("wedzarnia", NVS_READONLY, &nvsHandle) != ESP_OK) return;
    size_t len;
    len = sizeof(wifiStaSsid);
    if (nvs_get_str(nvsHandle, "wifi_ssid", wifiStaSsid, &len) != ESP_OK) wifiStaSsid[0] = '\0';
    len = sizeof(wifiStaPass);
    if (nvs_get_str(nvsHandle, "wifi_pass", wifiStaPass, &len) != ESP_OK) wifiStaPass[0] = '\0';
    len = sizeof(lastProfilePath);
    if (nvs_get_str(nvsHandle, "profile", lastProfilePath, &len) != ESP_OK) { strcpy(lastProfilePath, "/profiles/test.prof"); }
    state_lock();
    double tmp_d;
    len = sizeof(tmp_d);
    if (nvs_get_blob(nvsHandle, "manual_tset", &tmp_d, &len) == ESP_OK) g_tSet = tmp_d;
    int32_t tmp_i;
    if (nvs_get_i32(nvsHandle, "manual_pow", &tmp_i) == ESP_OK) g_powerMode = tmp_i;
    if (nvs_get_i32(nvsHandle, "manual_smoke", &tmp_i) == ESP_OK) g_manualSmokePwm = tmp_i;
    if (nvs_get_i32(nvsHandle, "manual_fan", &tmp_i) == ESP_OK) g_fanMode = tmp_i;
    state_unlock();
    nvs_close(nvsHandle);
    Serial.println("NVS config loaded");
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
    strncpy(wifiStaPass, pass, sizeof(wifiStaPass) - 1);
    nvs_save_generic([&](nvs_handle_t handle){
        nvs_set_str(handle, "wifi_ssid", wifiStaSsid);
        nvs_set_str(handle, "wifi_pass", wifiStaPass);
    });
}

void storage_save_profile_path_nvs(const char* path) {
    strncpy(lastProfilePath, path, sizeof(lastProfilePath) - 1);
     nvs_save_generic([&](nvs_handle_t handle){
        nvs_set_str(handle, "profile", lastProfilePath);
    });
}

void storage_save_manual_settings_nvs() {
    state_lock();
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
}

String storage_list_profiles_json() {
    String json = "[";
    bool first = true;
    File root = SD.open("/profiles");
    if (!root || !root.isDirectory()) {
        Serial.println("Nie można otworzyć katalogu /profiles");
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
    Serial.println("Znalezione profile: " + json);
    return json;
}

bool storage_reinit_sd() {
    Serial.println("Hard re-initializing SD card...");
    SD.end();
    delay(100);
    if (SD.begin(PIN_SD_CS)) {
        Serial.println("SD card re-initialized successfully.");
        return true;
    } else {
        Serial.println("Failed to re-initialize SD card.");
        return false;
    }
}

String storage_get_profile_as_json(const char* profileName) {
    String path = "/profiles/" + String(profileName);
    if (!SD.exists(path)) { return "[]"; }
    File f = SD.open(path, "r");
    if (!f) { return "[]"; }
    String json = "[";
    bool firstStep = true;
    char lineBuf[256];
    while (f.available()) {
        int len = f.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[len] = '\0';
        char* line = lineBuf;
        while (*line == ' ' || *line == '\t') line++;
        len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) { line[--len] = '\0'; }
        if (len == 0 || line[0] == '#') continue;
        char* fields[10];
        int fieldCount = 0;
        char* token = strtok(line, ";");
        while (token && fieldCount < 10) { fields[fieldCount++] = token; token = strtok(NULL, ";"); }
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
    if (WiFi.status() != WL_CONNECTED) return "[\"Brak WiFi\"]";
    HTTPClient http;
    http.begin(CFG_GITHUB_API_URL);
    http.addHeader("User-Agent", "ESP32-Wedzarnia-Client");
    int httpCode = http.GET();
    yield(); // <-- POPRAWKA
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return "[\"Blad API GitHub\"]";
    }
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getStream());
    http.end();
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
        state_lock(); g_errorProfile = true; state_unlock();
        return false;
    }
    HTTPClient http;
    String url = String(CFG_GITHUB_PROFILES_BASE_URL) + String(profileName);
    http.begin(url);
    int httpCode = http.GET();
    yield(); // <-- POPRAWKA
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        state_lock(); g_errorProfile = true; state_unlock();
        return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    int loadedStepCount = 0;
    char lineBuf[256];
    while (http.connected() && stream->available() && loadedStepCount < MAX_STEPS) {
        int len = stream->readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[len] = '\0';
        char* line = lineBuf;
        while (*line == ' ' || *line == '\t') line++;
        len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) { line[--len] = '\0'; }
        if (len == 0 || line[0] == '#') continue;
        char* fields[10];
        int fieldCount = 0;
        char* token = strtok(line, ";");
        while (token && fieldCount < 10) { fields[fieldCount++] = token; token = strtok(NULL, ";"); }
        if (fieldCount < 10) continue;
        Step& s = g_profile[loadedStepCount];
        strncpy(s.name, fields[0], sizeof(s.name) - 1);
        s.name[sizeof(s.name) - 1] = '\0';
        s.tSet = constrain(atof(fields[1]), CFG_T_MIN_SET, CFG_T_MAX_SET);
        s.tMeatTarget = constrain(atof(fields[2]), 0, 100);
        s.minTimeMs = (unsigned long)(atoi(fields[3])) * 60UL * 1000UL;
        s.powerMode = constrain(atoi(fields[4]), CFG_POWERMODE_MIN, CFG_POWERMODE_MAX);
        s.smokePwm = constrain(atoi(fields[5]), CFG_SMOKE_PWM_MIN, CFG_SMOKE_PWM_MAX);
        s.fanMode = constrain(atoi(fields[6]), 0, 2);
        s.fanOnTime = max(1000UL, (unsigned long)(atoi(fields[7])) * 1000UL);
        s.fanOffTime = max(1000UL, (unsigned long)(atoi(fields[8])) * 1000UL);
        s.useMeatTemp = parseBool(fields[9]);
        loadedStepCount++;
    }
    http.end();
    state_lock();
    g_stepCount = loadedStepCount;
    g_errorProfile = (g_stepCount == 0);
    state_unlock();
    Serial.printf("Profile '%s' loaded from GitHub: %d steps\n", profileName, g_stepCount);
    return !g_errorProfile;
}
