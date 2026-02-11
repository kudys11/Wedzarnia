// wifimanager.cpp - Implementacja zarządzania WiFi
#include "wifimanager.h"
#include "config.h"
#include "storage.h"
#include "outputs.h"
#include <WiFi.h>

// Zmienne prywatne
static unsigned long lastWiFiCheck = 0;
static unsigned long retryDelay = CFG_WIFI_RETRY_MIN_DELAY;
static bool wasConnected = false;
static WiFiStats stats = {0, 0, 0, 0, 0};
static unsigned long connectionStartTime = 0;
static unsigned long disconnectionStartTime = 0;

void wifi_init() {
    WiFi.mode(WIFI_AP_STA);
    
    // Uruchom Access Point
    WiFi.softAP(CFG_AP_SSID, CFG_AP_PASS);
    log_msg(APP_LOG_LEVEL_INFO, "AP Started: " + String(CFG_AP_SSID));
    log_msg(APP_LOG_LEVEL_INFO, "AP IP: " + WiFi.softAPIP().toString());
    
    // Spróbuj połączyć się z zapisaną siecią WiFi STA
    const char* sta_ssid = storage_get_wifi_ssid();
    const char* sta_pass = storage_get_wifi_pass();
    
    if (sta_ssid && strlen(sta_ssid) > 0) {
        log_msg(APP_LOG_LEVEL_INFO, "Connecting to WiFi: " + String(sta_ssid));
        WiFi.begin(sta_ssid, sta_pass);
        connectionStartTime = millis();
        
        // Czekaj maksymalnie 10 sekund na połączenie
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            log_msg(APP_LOG_LEVEL_INFO, "WiFi connected!");
            log_msg(APP_LOG_LEVEL_INFO, "STA IP: " + WiFi.localIP().toString());
            wasConnected = true;
            stats.lastReconnect = millis();
            buzzerBeep(2, 50, 50); // Krótki sygnał sukcesu
        } else {
            log_msg(APP_LOG_LEVEL_WARN, "WiFi connection failed");
            disconnectionStartTime = millis();
        }
    } else {
        log_msg(APP_LOG_LEVEL_INFO, "No WiFi credentials stored, running in AP-only mode");
    }
}

void wifi_maintain_connection() {
    unsigned long now = millis();
    
    // Sprawdzaj stan co CFG_WIFI_CHECK_INTERVAL
    if (now - lastWiFiCheck < CFG_WIFI_CHECK_INTERVAL) {
        return;
    }
    lastWiFiCheck = now;
    
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    
    // Aktualizacja statystyk
    if (isConnected && !wasConnected) {
        // Właśnie się połączyliśmy
        stats.lastReconnect = now;
        if (disconnectionStartTime > 0) {
            stats.totalDowntime += (now - disconnectionStartTime);
        }
        disconnectionStartTime = 0;
        connectionStartTime = now;
        retryDelay = CFG_WIFI_RETRY_MIN_DELAY; // Reset backoff
        log_msg(APP_LOG_LEVEL_INFO, "WiFi reconnected! IP: " + WiFi.localIP().toString());
        buzzerBeep(1, 50, 0); // Krótki sygnał
        
    } else if (!isConnected && wasConnected) {
        // Właśnie się rozłączyliśmy
        stats.disconnectCount++;
        stats.lastDisconnect = now;
        if (connectionStartTime > 0) {
            stats.totalUptime += (now - connectionStartTime);
        }
        connectionStartTime = 0;
        disconnectionStartTime = now;
        log_msg(APP_LOG_LEVEL_WARN, "WiFi connection lost!");
    }
    
    wasConnected = isConnected;
    
    // Auto-reconnect z exponential backoff
    if (!isConnected) {
        const char* sta_ssid = storage_get_wifi_ssid();
        
        if (sta_ssid && strlen(sta_ssid) > 0) {
            static unsigned long lastReconnectAttempt = 0;
            
            if (now - lastReconnectAttempt >= retryDelay) {
                lastReconnectAttempt = now;
                
                log_msg(APP_LOG_LEVEL_INFO, "Attempting WiFi reconnect (delay: " + 
                        String(retryDelay/1000) + "s)...");
                
                WiFi.reconnect();
                
                // Exponential backoff
                retryDelay = min(retryDelay * 2, CFG_WIFI_RETRY_MAX_DELAY);
            }
        }
    } else {
        // Jesteśmy połączeni - aktualizuj czas połączenia
        if (connectionStartTime > 0) {
            // Nic nie rób, totalUptime zostanie zaktualizowany przy rozłączeniu
        }
    }
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

WiFiStats wifi_get_stats() {
    unsigned long now = millis();
    WiFiStats currentStats = stats;
    
    // Dodaj aktualny czas połączenia/rozłączenia do statystyk
    if (wasConnected && connectionStartTime > 0) {
        currentStats.totalUptime = stats.totalUptime + (now - connectionStartTime);
    }
    if (!wasConnected && disconnectionStartTime > 0) {
        currentStats.totalDowntime = stats.totalDowntime + (now - disconnectionStartTime);
    }
    
    return currentStats;
}

void wifi_reset_stats() {
    stats.totalUptime = 0;
    stats.totalDowntime = 0;
    stats.disconnectCount = 0;
    stats.lastDisconnect = 0;
    stats.lastReconnect = 0;
    log_msg(APP_LOG_LEVEL_INFO, "WiFi stats reset");
}
