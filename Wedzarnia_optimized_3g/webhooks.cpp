// webhooks.cpp - Pełna implementacja integracji z Home Assistant
#include "webhooks.h"
#include "config.h"
#include "state.h"
#include "outputs.h"
#include "storage.h"
#include "wifimanager.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#ifdef ENABLE_HOME_ASSISTANT

// ======================================================
// ZMIENNE STATYCZNE
// ======================================================
static unsigned long lastUpdateTime = 0;
static unsigned long lastConnectionTest = 0;
static bool haInitialized = false;
static bool deviceRegistered = false;
static int connectionFailCount = 0;
static const int MAX_FAIL_COUNT = 3;

// Interwały (milisekundy)
static const unsigned long CONNECTION_TEST_INTERVAL = 60000;    // 60 sekund
static const unsigned long REGISTRATION_RETRY_INTERVAL = 300000; // 5 minut
static unsigned long lastRegistrationAttempt = 0;

// ======================================================
// FUNKCJE POMOCNICZE
// ======================================================

// Przygotuj nagłówki HTTP dla Home Assistant
static void prepare_ha_headers(HTTPClient& http) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
}

// Sprawdź odpowiedź HTTP
static bool check_http_response(int httpCode, const char* operation) {
    if (httpCode == 200 || httpCode == 201) {
        log_msg(LOG_LEVEL_DEBUG, String("HA ") + operation + ": OK (HTTP ") + String(httpCode) + ")");
        connectionFailCount = 0; // Reset fail count on success
        return true;
    } else if (httpCode == 401) {
        log_msg(LOG_LEVEL_ERROR, String("HA ") + operation + ": UNAUTHORIZED - check your token!");
        connectionFailCount++;
        return false;
    } else if (httpCode == 404) {
        log_msg(LOG_LEVEL_WARN, String("HA ") + operation + ": NOT FOUND - check URL");
        connectionFailCount++;
        return false;
    } else {
        log_msg(LOG_LEVEL_WARN, String("HA ") + operation + " failed: HTTP " + String(httpCode));
        connectionFailCount++;
        return false;
    }
}

// Utwórz unikalny identyfikator urządzenia
static String get_device_unique_id() {
    uint64_t chipId = ESP.getEfuseMac();
    char uniqueId[32];
    snprintf(uniqueId, sizeof(uniqueId), "wedzarnia_esp32_%04X%08X", 
             (uint16_t)(chipId >> 32), (uint32_t)chipId);
    return String(uniqueId);
}

// ======================================================
// TEST POŁĄCZENIA Z HOME ASSISTANT
// ======================================================

bool ha_test_connection() {
    if (!wifi_is_connected()) {
        log_msg(LOG_LEVEL_WARN, "HA test: WiFi not connected");
        return false;
    }
    
    HTTPClient http;
    String url = String(HA_SERVER) + "/api/";
    
    log_msg(LOG_LEVEL_DEBUG, "Testing HA connection to: " + url);
    
    http.begin(url);
    prepare_ha_headers(http);
    http.setTimeout(5000); // 5 sekund timeout
    
    int httpCode = http.GET();
    bool connected = check_http_response(httpCode, "connection test");
    
    http.end();
    
    if (connected) {
        log_msg(LOG_LEVEL_INFO, "Successfully connected to Home Assistant");
    } else if (connectionFailCount >= MAX_FAIL_COUNT) {
        log_msg(LOG_LEVEL_ERROR, "Multiple HA connection failures, pausing updates");
    }
    
    return connected;
}

// ======================================================
// REJESTRACJA URZĄDZENIA W HOME ASSISTANT
// ======================================================

void ha_register_device() {
    if (deviceRegistered || !wifi_is_connected()) {
        return;
    }
    
    unsigned long now = millis();
    if (now - lastRegistrationAttempt < REGISTRATION_RETRY_INTERVAL) {
        return;
    }
    
    lastRegistrationAttempt = now;
    
    // Najpierw przetestuj połączenie
    if (!ha_test_connection()) {
        log_msg(LOG_LEVEL_WARN, "Skipping device registration - no HA connection");
        return;
    }
    
    log_msg(LOG_LEVEL_INFO, "Attempting to register device in Home Assistant...");
    
    // 1. Stwórz konfigurację MQTT device (nawet jeśli nie używamy MQTT, HA to akceptuje)
    HTTPClient http;
    String url = String(HA_SERVER) + "/api/config/config_entries/flow";
    
    http.begin(url);
    prepare_ha_headers(http);
    
    String deviceId = get_device_unique_id();
    
    // Przygotuj dane urządzenia w formacie MQTT discovery
    String deviceJson = "{";
    deviceJson += "\"handler\":\"mqtt\",";
    deviceJson += "\"show_advanced_options\":false,";
    deviceJson += "\"title\":\"Wędzarnia ESP32\",";
    deviceJson += "\"data\":{";
    deviceJson += "\"device\":{";
    deviceJson += "\"identifiers\":[\"" + deviceId + "\"],";
    deviceJson += "\"name\":\"Wędzarnia na działce\",";
    deviceJson += "\"model\":\"ESP32 Wędzarnia Controller v3.0\",";
    deviceJson += "\"manufacturer\":\"DIY\",";
    deviceJson += "\"sw_version\":\"3.0.0\",";
    deviceJson += "\"configuration_url\":\"http://" + WiFi.localIP().toString() + "\"";
    deviceJson += "},";
    deviceJson += "\"discovery_prefix\":\"homeassistant\"";
    deviceJson += "}";
    deviceJson += "}";
    
    int httpCode = http.POST(deviceJson);
    
    if (check_http_response(httpCode, "device registration")) {
        deviceRegistered = true;
        log_msg(LOG_LEVEL_INFO, "Device successfully registered in Home Assistant");
        log_msg(LOG_LEVEL_INFO, "Device ID: " + deviceId);
    } else {
        log_msg(LOG_LEVEL_WARN, "Device registration failed, will retry later");
    }
    
    http.end();
}

// ======================================================
// AKTUALIZACJA ENTITY W HOME ASSISTANT
// ======================================================

static bool ha_update_entity(const char* entityId, const char* state, const char* jsonAttributes = "") {
    if (!wifi_is_connected() || connectionFailCount >= MAX_FAIL_COUNT) {
        return false;
    }
    
    HTTPClient http;
    String url = String(HA_SERVER) + "/api/states/" + entityId;
    
    http.begin(url);
    prepare_ha_headers(http);
    http.setTimeout(10000); // 10 sekund timeout
    
    // Przygotuj JSON
    String json = "{";
    json += "\"state\":\"" + String(state) + "\"";
    
    if (jsonAttributes && strlen(jsonAttributes) > 0) {
        json += ",\"attributes\":" + String(jsonAttributes);
    } else {
        json += ",\"attributes\":{}";
    }
    
    json += "}";
    
    int httpCode = http.PUT(json);
    bool success = check_http_response(httpCode, "update entity");
    
    http.end();
    return success;
}

// ======================================================
// FUNKCJE PUBLICZNE
// ======================================================

void ha_webhook_init() {
    log_msg(LOG_LEVEL_INFO, "Initializing Home Assistant integration");
    log_msg(LOG_LEVEL_INFO, String("HA Server: ") + HA_SERVER);
    log_msg(LOG_LEVEL_INFO, String("Update interval: ") + String(HA_UPDATE_INTERVAL / 1000) + "s");
    
    haInitialized = true;
    
    // Opóźnij pierwszą rejestrację, żeby WiFi miał czas się połączyć
    lastRegistrationAttempt = millis() - REGISTRATION_RETRY_INTERVAL + 10000;
}

void ha_webhook_loop() {
    if (!haInitialized || !wifi_is_connected()) {
        return;
    }
    
    unsigned long now = millis();
    
    // Test połączenia z HA (co minutę)
    if (now - lastConnectionTest > CONNECTION_TEST_INTERVAL) {
        lastConnectionTest = now;
        ha_test_connection();
    }
    
    // Spróbuj zarejestrować urządzenie jeśli jeszcze nie zarejestrowane
    if (!deviceRegistered) {
        ha_register_device();
    }
    
    // Regularne wysyłanie danych (co zdefiniowany interwał)
    if (now - lastUpdateTime > HA_UPDATE_INTERVAL && connectionFailCount < MAX_FAIL_COUNT) {
        lastUpdateTime = now;
        
        // Pobierz dane ze stanu
        double tc = 0, tm = 0, ts = 0;
        ProcessState st = ProcessState::IDLE;
        
        if (state_lock()) {
            tc = g_tChamber;
            tm = g_tMeat;
            ts = g_tSet;
            st = g_currentState;
            state_unlock();
            
            // Wyślij temperatury
            ha_send_temperature(tc, tm, ts);
            
            // Wyślij stan
            const char* stateStr = "unknown";
            const char* stateIcon = "mdi:help";
            
            switch (st) {
                case ProcessState::IDLE: 
                    stateStr = "idle"; 
                    stateIcon = "mdi:sleep";
                    break;
                case ProcessState::RUNNING_AUTO: 
                    stateStr = "running_auto"; 
                    stateIcon = "mdi:fire";
                    break;
                case ProcessState::RUNNING_MANUAL: 
                    stateStr = "running_manual"; 
                    stateIcon = "mdi:fire";
                    break;
                case ProcessState::PAUSE_DOOR: 
                    stateStr = "pause_door"; 
                    stateIcon = "mdi:door-open";
                    break;
                case ProcessState::PAUSE_SENSOR: 
                    stateStr = "pause_sensor"; 
                    stateIcon = "mdi:alert-circle";
                    break;
                case ProcessState::PAUSE_OVERHEAT: 
                    stateStr = "pause_overheat"; 
                    stateIcon = "mdi:thermometer-alert";
                    break;
                case ProcessState::PAUSE_USER: 
                    stateStr = "pause_user"; 
                    stateIcon = "mdi:pause";
                    break;
                case ProcessState::ERROR_PROFILE: 
                    stateStr = "error_profile"; 
                    stateIcon = "mdi:alert";
                    break;
                case ProcessState::SOFT_RESUME: 
                    stateStr = "soft_resume"; 
                    stateIcon = "mdi:play";
                    break;
            }
            
            ha_send_state(stateStr, stateIcon);
        }
    }
}

void ha_send_temperature(float chamber, float meat, float setpoint) {
    // 1. Temperatura komory (główny sensor)
    String attrsChamber = "{";
    attrsChamber += "\"unit_of_measurement\":\"°C\",";
    attrsChamber += "\"device_class\":\"temperature\",";
    attrsChamber += "\"state_class\":\"measurement\",";
    attrsChamber += "\"friendly_name\":\"Wędzarnia - Temperatura Komory\",";
    attrsChamber += "\"icon\":\"mdi:thermometer\",";
    attrsChamber += "\"meat_temperature\":" + String(meat, 1) + ",";
    attrsChamber += "\"setpoint\":" + String(setpoint, 1) + ",";
    attrsChamber += "\"device_id\":\"" + get_device_unique_id() + "\"";
    attrsChamber += "}";
    
    ha_update_entity(HA_ENTITY_TEMP, String(chamber, 1).c_str(), attrsChamber.c_str());
    
    // 2. Temperatura mięsa (osobny sensor)
    String attrsMeat = "{";
    attrsMeat += "\"unit_of_measurement\":\"°C\",";
    attrsMeat += "\"device_class\":\"temperature\",";
    attrsMeat += "\"state_class\":\"measurement\",";
    attrsMeat += "\"friendly_name\":\"Wędzarnia - Temperatura Mięsa\",";
    attrsMeat += "\"icon\":\"mdi:food-drumstick\",";
    attrsMeat += "\"chamber_temperature\":" + String(chamber, 1) + ",";
    attrsMeat += "\"setpoint\":" + String(setpoint, 1) + ",";
    attrsMeat += "\"device_id\":\"" + get_device_unique_id() + "\"";
    attrsMeat += "}";
    
    ha_update_entity(HA_ENTITY_MEAT, String(meat, 1).c_str(), attrsMeat.c_str());
}

void ha_send_state(const char* state, const char* icon) {
    // Użyj domyślnej ikony jeśli nie podano
    const char* stateIcon = icon ? icon : "mdi:grill";
    
    String attrs = "{";
    attrs += "\"friendly_name\":\"Wędzarnia - Stan\",";
    attrs += "\"icon\":\"" + String(stateIcon) + "\",";
    
    // Dodaj dodatkowe atrybuty w zależności od stanu
    if (strcmp(state, "running_auto") == 0 || strcmp(state, "running_manual") == 0) {
        // Pobierz czas działania jeśli proces aktywny
        unsigned long runtime = 0;
        if (state_lock()) {
            if (g_currentState == ProcessState::RUNNING_AUTO || 
                g_currentState == ProcessState::RUNNING_MANUAL) {
                runtime = (millis() - g_processStartTime) / 1000;
            }
            state_unlock();
        }
        
        attrs += "\"runtime_seconds\":" + String(runtime) + ",";
        attrs += "\"runtime_formatted\":\"" + format_time(runtime) + "\",";
    }
    
    attrs += "\"timestamp\":" + String(millis() / 1000) + ",";
    attrs += "\"device_id\":\"" + get_device_unique_id() + "\"";
    attrs += "}";
    
    ha_update_entity(HA_ENTITY_STATE, state, attrs.c_str());
}

void ha_send_alert(const char* title, const char* message) {
    if (!wifi_is_connected() || connectionFailCount >= MAX_FAIL_COUNT) {
        return;
    }
    
    log_msg(LOG_LEVEL_INFO, String("HA Alert: ") + title + " - " + message);
    
    // 1. Wyślij jako persistent notification
    HTTPClient http;
    String url = String(HA_SERVER) + "/api/services/persistent_notification/create";
    
    http.begin(url);
    prepare_ha_headers(http);
    
    String notificationJson = "{";
    notificationJson += "\"title\":\"Wędzarnia: " + String(title) + "\",";
    notificationJson += "\"message\":\"" + String(message) + "\",";
    notificationJson += "\"notification_id\":\"wedzarnia_alert_" + String(millis() / 1000) + "\"";
    notificationJson += "}";
    
    int httpCode = http.POST(notificationJson);
    check_http_response(httpCode, "send alert notification");
    
    http.end();
    
    // 2. Również zaktualizuj stan entity
    String alertAttrs = "{";
    alertAttrs += "\"friendly_name\":\"Wędzarnia - Stan\",";
    alertAttrs += "\"icon\":\"mdi:alert\",";
    alertAttrs += "\"alert_title\":\"" + String(title) + "\",";
    alertAttrs += "\"alert_message\":\"" + String(message) + "\",";
    alertAttrs += "\"timestamp\":" + String(millis() / 1000) + ",";
    alertAttrs += "\"device_id\":\"" + get_device_unique_id() + "\"";
    alertAttrs += "}";
    
    ha_update_entity(HA_ENTITY_STATE, "alert", alertAttrs.c_str());
    
    // 3. Wyzwól webhook dla automatyzacji (jeśli skonfigurowany)
    if (strlen(HA_WEBHOOK_ID) > 0) {
        ha_trigger_webhook(HA_WEBHOOK_ID, message);
    }
}

void ha_trigger_webhook(const char* webhook_id, const char* data) {
    if (!wifi_is_connected()) {
        return;
    }
    
    HTTPClient http;
    String url = String(HA_SERVER) + "/api/webhook/" + webhook_id;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);
    
    String json = "{";
    json += "\"device\":\"wedzarnia_esp32\",";
    json += "\"device_id\":\"" + get_device_unique_id() + "\",";
    json += "\"timestamp\":" + String(millis() / 1000) + ",";
    json += "\"data\":\"" + String(data) + "\"";
    json += "}";
    
    int httpCode = http.POST(json);
    check_http_response(httpCode, "trigger webhook");
    
    http.end();
}

// Pomocnicza funkcja do formatowania czasu
static String format_time(unsigned long seconds) {
    unsigned long hours = seconds / 3600;
    unsigned long minutes = (seconds % 3600) / 60;
    unsigned long secs = seconds % 60;
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, secs);
    return String(buffer);
}

// Dodatkowa funkcja: wysyłanie statystyk
void ha_send_statistics() {
    if (!wifi_is_connected() || connectionFailCount >= MAX_FAIL_COUNT) {
        return;
    }
    
    ProcessStats stats;
    if (state_lock()) {
        stats = g_processStats;
        state_unlock();
    } else {
        return;
    }
    
    // Stwórz entity dla statystyk
    String statsEntity = "sensor." + get_device_unique_id() + "_stats";
    
    String attrs = "{";
    attrs += "\"friendly_name\":\"Wędzarnia - Statystyki\",";
    attrs += "\"icon\":\"mdi:chart-line\",";
    attrs += "\"total_runtime_seconds\":" + String(stats.totalRunTime / 1000) + ",";
    attrs += "\"heating_percentage\":" + 
             String(stats.totalRunTime > 0 ? (stats.activeHeatingTime * 100) / stats.totalRunTime : 0) + ",";
    attrs += "\"average_temperature\":" + String(stats.avgTemp, 1) + ",";
    attrs += "\"step_changes\":" + String(stats.stepChanges) + ",";
    attrs += "\"pause_count\":" + String(stats.pauseCount) + ",";
    attrs += "\"device_id\":\"" + get_device_unique_id() + "\"";
    attrs += "}";
    
    ha_update_entity(statsEntity.c_str(), "stats", attrs.c_str());
}

// Funkcja do ręcznego wysłania wszystkich danych
void ha_send_all_data() {
    if (!wifi_is_connected()) {
        return;
    }
    
    double tc, tm, ts;
    ProcessState st;
    
    if (state_lock()) {
        tc = g_tChamber;
        tm = g_tMeat;
        ts = g_tSet;
        st = g_currentState;
        state_unlock();
    } else {
        return;
    }
    
    log_msg(LOG_LEVEL_INFO, "Sending all data to Home Assistant");
    
    ha_send_temperature(tc, tm, ts);
    
    const char* stateStr = "unknown";
    const char* stateIcon = "mdi:help";
    
    switch (st) {
        case ProcessState::IDLE: stateStr = "idle"; stateIcon = "mdi:sleep"; break;
        case ProcessState::RUNNING_AUTO: stateStr = "running_auto"; stateIcon = "mdi:fire"; break;
        case ProcessState::RUNNING_MANUAL: stateStr = "running_manual"; stateIcon = "mdi:fire"; break;
        case ProcessState::PAUSE_DOOR: stateStr = "pause_door"; stateIcon = "mdi:door-open"; break;
        case ProcessState::PAUSE_SENSOR: stateStr = "pause_sensor"; stateIcon = "mdi:alert-circle"; break;
        case ProcessState::PAUSE_OVERHEAT: stateStr = "pause_overheat"; stateIcon = "mdi:thermometer-alert"; break;
        case ProcessState::PAUSE_USER: stateStr = "pause_user"; stateIcon = "mdi:pause"; break;
        case ProcessState::ERROR_PROFILE: stateStr = "error_profile"; stateIcon = "mdi:alert"; break;
        case ProcessState::SOFT_RESUME: stateStr = "soft_resume"; stateIcon = "mdi:play"; break;
    }
    
    ha_send_state(stateStr, stateIcon);
    ha_send_statistics();
    
    log_msg(LOG_LEVEL_INFO, "All data sent to Home Assistant");
}

// Sprawdź czy Home Assistant integration jest aktywna
bool ha_is_connected() {
    return wifi_is_connected() && (connectionFailCount < MAX_FAIL_COUNT);
}

// Zresetuj licznik błędów (np. po zmianie konfiguracji)
void ha_reset_connection() {
    connectionFailCount = 0;
    deviceRegistered = false;
    lastRegistrationAttempt = 0;
    log_msg(LOG_LEVEL_INFO, "HA connection reset");
}

#endif // ENABLE_HOME_ASSISTANT