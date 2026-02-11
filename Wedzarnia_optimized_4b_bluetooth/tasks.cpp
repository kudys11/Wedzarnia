#include "tasks.h"
#include "config.h"
#include "state.h"
#include "process.h"
#include "sensors.h"
#include "ui.h"
#include "outputs.h"
#include "web_server.h"
#include "wifimanager.h"
#include <esp_task_wdt.h>

// Konfiguruje wbudowany w ESP32 watchdog (TWDT).
// Słowo 'static' zostało usunięte, aby funkcja była dostępna z pliku .ino
void watchdog_init() {
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true 
    };
    // Używamy reconfigure, na wypadek gdyby WDT był już zainicjowany przez system
    esp_task_wdt_reconfigure(&wdt_config);
    log_msg(APP_LOG_LEVEL_INFO, "Task Watchdog reconfigured (" + String(WDT_TIMEOUT) + "s timeout)");
}

void taskControl(void* pv) {
    log_msg(APP_LOG_LEVEL_INFO, "Control task started");
    esp_task_wdt_add(NULL); // Dodaje bieżące zadanie do watchdoga

    for (;;) {
        esp_task_wdt_reset(); // Resetuje watchdog
        process_run_control_logic();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void taskSensors(void* pv) {
    log_msg(APP_LOG_LEVEL_INFO, "Sensors task started");
    esp_task_wdt_add(NULL); // Dodaje bieżące zadanie do watchdoga

    for (;;) {
        esp_task_wdt_reset(); // Resetuje watchdog
        requestTemperature();
        readTemperature();
        checkDoor();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void taskUI(void* pv) {
    log_msg(APP_LOG_LEVEL_INFO, "UI task started");
    esp_task_wdt_add(NULL); // Dodaje bieżące zadanie do watchdoga

    for (;;) {
        esp_task_wdt_reset(); // Resetuje watchdog
        ui_handle_buttons();
        handleBuzzer();
        ui_update_display();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void taskWeb(void* pv) {
    log_msg(APP_LOG_LEVEL_INFO, "Web task started");
    esp_task_wdt_add(NULL); // Dodaje bieżące zadanie do watchdoga

    for (;;) {
        esp_task_wdt_reset(); // Resetuje watchdog
        web_server_handle_client();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void taskWiFi(void* pv) {
    log_msg(APP_LOG_LEVEL_INFO, "WiFi task started");
    esp_task_wdt_add(NULL); // Dodaje bieżące zadanie do watchdoga

    for (;;) {
        esp_task_wdt_reset(); // Resetuje watchdog
        wifi_maintain_connection();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void taskMonitor(void* pv) {
    log_msg(APP_LOG_LEVEL_INFO, "Monitor task started");
    esp_task_wdt_add(NULL); // Dodaje bieżące zadanie do watchdoga

    unsigned long lastHeapLog = 0;
    unsigned long lastStatsLog = 0;

    for (;;) {
        esp_task_wdt_reset(); // Resetuje watchdog

        unsigned long now = millis();

        // Logowanie użycia pamięci co minutę
        if (now - lastHeapLog > 60000) {
            lastHeapLog = now;
            uint32_t freeHeap = ESP.getFreeHeap();
            uint32_t minHeap = ESP.getMinFreeHeap();
            log_msg(APP_LOG_LEVEL_INFO, "[HEAP] Free: " + String(freeHeap) + " B, Min: " + String(minHeap) + " B");

            if (freeHeap < 20000) {
                log_msg(APP_LOG_LEVEL_WARN, "!!! LOW MEMORY WARNING !!!");
                buzzerBeep(2, 100, 100);
            }
        }

        // Logowanie statystyk procesu i WiFi co 5 minut
        if (now - lastStatsLog > 300000) {
            lastStatsLog = now;
            
            if (state_lock()) {
                ProcessStats stats = g_processStats;
                state_unlock();
                
                if (stats.totalRunTime > 0) {
                    unsigned long runHours = stats.totalRunTime / 3600000;
                    unsigned long runMins = (stats.totalRunTime % 3600000) / 60000;
                    unsigned long heatPercent = (stats.totalRunTime > 0) ? (stats.activeHeatingTime * 100) / stats.totalRunTime : 0;
                    
                    log_msg(APP_LOG_LEVEL_INFO, "[STATS] Runtime: " + String(runHours) + "h " + String(runMins) + "m");
                    log_msg(APP_LOG_LEVEL_INFO, "[STATS] Heating: " + String(heatPercent) + "%, Avg temp: " + String(stats.avgTemp, 1) + "°C");
                    log_msg(APP_LOG_LEVEL_INFO, "[STATS] Steps: " + String(stats.stepChanges) + ", Pauses: " + String(stats.pauseCount));
                }
            }
            
            if (wifi_is_connected()) {
                WiFiStats wifiStats = wifi_get_stats();
                unsigned long upHours = wifiStats.totalUptime / 3600000;
                unsigned long downHours = wifiStats.totalDowntime / 3600000;
                log_msg(APP_LOG_LEVEL_INFO, "[WiFi] Up: " + String(upHours) + "h, Down: " + String(downHours) + "h, Disconnects: " + String(wifiStats.disconnectCount));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void tasks_create_all() {
    // Wywołanie watchdog_init() zostało przeniesione do głównej funkcji setup() w pliku .ino
    // Jest to kluczowe, aby watchdog był gotowy przed inicjalizacją innych bibliotek (np. BLE).
    
    // Core 1: Zadania krytyczne (interfejs, sterowanie, czujniki)
    xTaskCreatePinnedToCore(taskControl, "Control", 8192, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(taskSensors, "Sensors", 8192, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskUI, "UI", 8192, NULL, 2, NULL, 1);
    
    // Core 0: Zadania sieciowe i monitoring
    xTaskCreatePinnedToCore(taskWeb, "Web", 12288, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskWiFi, "WiFi", 8192, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskMonitor, "Monitor", 8192, NULL, 1, NULL, 0);
    
    log_msg(APP_LOG_LEVEL_INFO, "All tasks created successfully");
}
