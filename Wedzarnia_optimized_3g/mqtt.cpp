// mqtt.cpp - Komunikacja MQTT dla zdalnego monitorowania
#include "mqtt.h"
#include "config.h"
#include "state.h"
#include "process.h"
#include "sensors.h"
#include "outputs.h"
#include "storage.h"
#include "wifimanager.h"
#include <PubSubClient.h>
#include <WiFi.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

static unsigned long lastMqttPublish = 0;
static constexpr unsigned long MQTT_PUBLISH_INTERVAL = 10000; // 10 sekund
static char mqttClientId[32];
static bool mqttEnabled = false;

// Callback dla przychodzących wiadomości MQTT
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    log_msg(LOG_LEVEL_INFO, "MQTT message: " + String(topic) + " = " + message);
    
    // Analiza topicu
    String topicStr = String(topic);
    
    if (topicStr.endsWith("/control/start_auto")) {
        process_start_auto();
    } 
    else if (topicStr.endsWith("/control/start_manual")) {
        process_start_manual();
    }
    else if (topicStr.endsWith("/control/stop")) {
        allOutputsOff();
        if (state_lock()) {
            g_currentState = ProcessState::IDLE;
            state_unlock();
        }
    }
    else if (topicStr.endsWith("/control/set_temp")) {
        double temp = message.toFloat();
        temp = constrain(temp, CFG_T_MIN_SET, CFG_T_MAX_SET);
        if (state_lock()) {
            g_tSet = temp;
            state_unlock();
        }
        storage_save_manual_settings_nvs();
    }
    else if (topicStr.endsWith("/control/next_step")) {
        process_force_next_step();
    }
    else if (topicStr.endsWith("/control/reset_pid")) {
        resetAdaptivePid();
    }
}

void mqtt_init() {
    // Wygeneruj unikalny client ID
    uint64_t chipId = ESP.getEfuseMac();
    snprintf(mqttClientId, sizeof(mqttClientId), "wedzarnia_%04X", (uint16_t)(chipId >> 32));
    
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512); // Zwiększony bufor dla większych wiadomości
    
    mqttEnabled = true;
    log_msg(LOG_LEVEL_INFO, "MQTT initialized, client ID: " + String(mqttClientId));
}

bool mqtt_reconnect() {
    if (!mqttEnabled || !wifi_is_connected()) return false;
    
    if (mqttClient.connected()) return true;
    
    log_msg(LOG_LEVEL_INFO, "Attempting MQTT connection...");
    
    // Próba połączenia z will message
    String willTopic = String(MQTT_TOPIC_PREFIX) + "/" + mqttClientId + "/status";
    if (mqttClient.connect(mqttClientId, 
                          willTopic.c_str(), 
                          0, // QoS 0
                          true, // retain
                          "offline")) {
        
        log_msg(LOG_LEVEL_INFO, "MQTT connected");
        
        // Subskrybuj topic kontrolny
        String controlTopic = String(MQTT_TOPIC_PREFIX) + "/" + mqttClientId + "/control/#";
        mqttClient.subscribe(controlTopic.c_str());
        
        // Opublikuj status online
        mqttClient.publish(willTopic.c_str(), "online", true);
        
        // Opublikuj informacje o urządzeniu
        mqtt_publish_device_info();
        
        return true;
    } else {
        log_msg(LOG_LEVEL_ERROR, "MQTT connection failed, rc=" + String(mqttClient.state()));
        return false;
    }
}

void mqtt_loop() {
    if (!mqttEnabled) return;
    
    if (!mqttClient.connected()) {
        static unsigned long lastReconnectAttempt = 0;
        unsigned long now = millis();
        
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            mqtt_reconnect();
        }
    } else {
        mqttClient.loop();
        
        // Regularne publikowanie statusu
        unsigned long now = millis();
        if (now - lastMqttPublish > MQTT_PUBLISH_INTERVAL) {
            lastMqttPublish = now;
            mqtt_publish_status();
        }
    }
}

void mqtt_publish_status() {
    if (!mqttClient.connected()) return;
    
    // Pobierz dane stanu
    double tc, tm, ts;
    int pm, fm, sm;
    ProcessState st;
    unsigned long uptime;
    
    state_lock();
    st = g_currentState;
    tc = g_tChamber;
    tm = g_tMeat;
    ts = g_tSet;
    pm = g_powerMode;
    fm = g_fanMode;
    sm = g_manualSmokePwm;
    uptime = millis() / 1000;
    state_unlock();
    
    // Topic base
    String topicBase = String(MQTT_TOPIC_PREFIX) + "/" + mqttClientId;
    
    // Publikuj poszczególne wartości
    mqttClient.publish((topicBase + "/temperature/chamber").c_str(), 
                      String(tc, 1).c_str(), true);
    mqttClient.publish((topicBase + "/temperature/meat").c_str(), 
                      String(tm, 1).c_str(), true);
    mqttClient.publish((topicBase + "/temperature/setpoint").c_str(), 
                      String(ts, 1).c_str(), true);
    mqttClient.publish((topicBase + "/power/mode").c_str(), 
                      String(pm).c_str(), true);
    mqttClient.publish((topicBase + "/fan/mode").c_str(), 
                      String(fm).c_str(), true);
    mqttClient.publish((topicBase + "/smoke/pwm").c_str(), 
                      String(sm).c_str(), true);
    
    // Publikuj stan jako string
    const char* stateStr = "unknown";
    switch (st) {
        case ProcessState::IDLE: stateStr = "idle"; break;
        case ProcessState::RUNNING_AUTO: stateStr = "running_auto"; break;
        case ProcessState::RUNNING_MANUAL: stateStr = "running_manual"; break;
        case ProcessState::PAUSE_DOOR: stateStr = "pause_door"; break;
        case ProcessState::PAUSE_SENSOR: stateStr = "pause_sensor"; break;
        case ProcessState::PAUSE_OVERHEAT: stateStr = "pause_overheat"; break;
        case ProcessState::PAUSE_USER: stateStr = "pause_user"; break;
        case ProcessState::ERROR_PROFILE: stateStr = "error_profile"; break;
        case ProcessState::SOFT_RESUME: stateStr = "soft_resume"; break;
    }
    
    mqttClient.publish((topicBase + "/state").c_str(), stateStr, true);
    mqttClient.publish((topicBase + "/uptime").c_str(), String(uptime).c_str(), true);
    
    // Publikuj JSON z wszystkimi danymi
    char jsonBuffer[512];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{\"chamber\":%.1f,\"meat\":%.1f,\"setpoint\":%.1f,\"power\":%d,"
        "\"fan\":%d,\"smoke\":%d,\"state\":\"%s\",\"uptime\":%lu}",
        tc, tm, ts, pm, fm, sm, stateStr, uptime);
    
    mqttClient.publish((topicBase + "/status").c_str(), jsonBuffer, false);
}

void mqtt_publish_device_info() {
    if (!mqttClient.connected()) return;
    
    String topicBase = String(MQTT_TOPIC_PREFIX) + "/" + mqttClientId;
    
    char infoBuffer[256];
    snprintf(infoBuffer, sizeof(infoBuffer),
        "{\"id\":\"%s\",\"type\":\"wedzarnia\",\"version\":\"3.2\","
        "\"heap\":%d,\"ip\":\"%s\"}",
        mqttClientId, ESP.getFreeHeap(), WiFi.localIP().toString().c_str());
    
    mqttClient.publish((topicBase + "/info").c_str(), infoBuffer, true);
}

void mqtt_publish_alert(const char* type, const char* message) {
    if (!mqttClient.connected()) return;
    
    String topicBase = String(MQTT_TOPIC_PREFIX) + "/" + mqttClientId;
    String alertTopic = topicBase + "/alerts/" + String(type);
    
    char alertBuffer[256];
    snprintf(alertBuffer, sizeof(alertBuffer),
        "{\"type\":\"%s\",\"message\":\"%s\",\"timestamp\":%lu}",
        type, message, millis() / 1000);
    
    mqttClient.publish(alertTopic.c_str(), alertBuffer, false);
}

void mqtt_set_enabled(bool enabled) {
    mqttEnabled = enabled;
    if (!enabled && mqttClient.connected()) {
        mqttClient.disconnect();
    }
    log_msg(LOG_LEVEL_INFO, "MQTT " + String(enabled ? "enabled" : "disabled"));
}

bool mqtt_is_connected() {
    return mqttClient.connected();
}