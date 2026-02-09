// tasks.cpp - Oczyszczona wersja (bez MQTT/HA)
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

// Watchdog dla zadań
struct TaskWatchdog {
    TickType_t lastReset;
    bool timeoutDetected;
    const char* taskName;
};

static TaskWatchdog taskWatchdogs[] = {
    {0, false, "Control"},
    {0, false, "Sensors"},
    {0, false, "UI"},
    {0, false, "Web"},
    {0, false, "WiFi"},
    {0, false, "Monitor"}
};

static void watchdog_init() {
    esp_task_wdt_config_t wdt_config = { 
        .timeout_ms = WDT_TIMEOUT * 1000, 
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, 
        .trigger_panic = true 
    };
    esp_task_wdt_reconfigure(&wdt_config);
    log_msg(LOG_LEVEL_INFO, "Watchdog initialized (" + String(WDT_TIMEOUT) + "s timeout)");
}

static void checkTaskWatchdog(int taskIndex) {
    TaskWatchdog& wd = taskWatchdogs[taskIndex];
    TickType_t now = xTaskGetTickCount();
    
    if (now - wd.lastReset > pdMS_TO_TICKS(TASK_WATCHDOG_TIMEOUT)) {
        if (!wd.timeoutDetected) {
            wd.timeoutDetected = true;
            log_msg(LOG_LEVEL_ERROR, String(wd.taskName) + " task timeout detected!");
            
            // Przywróć bezpieczny stan dla krytycznych zadań
            if (taskIndex == 0) { // Control task
                allOutputsOff();
                if (state_lock()) {
                    g_currentState = ProcessState::IDLE;
                    state_unlock();
                }
            }
        }
    } else {
        wd.timeoutDetected = false;
    }
}

void taskControl(void* pv) {
    esp_task_wdt_add(NULL);
    int taskIndex = 0;
    taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
    
    log_msg(LOG_LEVEL_INFO, "Control task started");
    
    for (;;) {
        esp_task_wdt_reset();
        taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
        
        process_run_control_logic();
        
        checkTaskWatchdog(taskIndex);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void taskSensors(void* pv) {
    esp_task_wdt_add(NULL);
    int taskIndex = 1;
    taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
    
    log_msg(LOG_LEVEL_INFO, "Sensors task started");
    
    for (;;) {
        esp_task_wdt_reset();
        taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
        
        requestTemperature();
        readTemperature();
        checkDoor();
        
        checkTaskWatchdog(taskIndex);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void taskUI(void* pv) {
    esp_task_wdt_add(NULL);
    int taskIndex = 2;
    taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
    
    log_msg(LOG_LEVEL_INFO, "UI task started");
    
    for (;;) {
        esp_task_wdt_reset();
        taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
        
        ui_handle_buttons();
        handleBuzzer();
        ui_update_display();
        
        checkTaskWatchdog(taskIndex);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void taskWeb(void* pv) {
    esp_task_wdt_add(NULL);
    int taskIndex = 3;
    taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
    
    log_msg(LOG_LEVEL_INFO, "Web task started");
    
    for (;;) {
        esp_task_wdt_reset();
        taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
        
        web_server_handle_client();
        
        checkTaskWatchdog(taskIndex);
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void taskWiFi(void* pv) {
    esp_task_wdt_add(NULL);
    int taskIndex = 4;
    taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
    
    log_msg(LOG_LEVEL_INFO, "WiFi task started");
    
    for (;;) {
        esp_task_wdt_reset();
        taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
        
        wifi_maintain_connection();
        
        checkTaskWatchdog(taskIndex);
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void taskMonitor(void* pv) {
    esp_task_wdt_add(NULL);
    int taskIndex = 5;
    taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
    
    log_msg(LOG_LEVEL_INFO, "Monitor task started");
    
    unsigned long lastHeapLog = 0;
    unsigned long lastStatsLog = 0;
    unsigned long lastWatchdogCheck = 0;
    
    for (;;) {
        esp_task_wdt_reset();
        taskWatchdogs[taskIndex].lastReset = xTaskGetTickCount();
        
        unsigned long now = millis();
        
        // Log heap memory co minutę
        if (now - lastHeapLog > 60000) {
            lastHeapLog = now;
            uint32_t freeHeap = ESP.getFreeHeap();
            uint32_t minHeap = ESP.getMinFreeHeap();
            
            log_msg(LOG_LEVEL_INFO, "[HEAP] Free: " + String(freeHeap) + " B, Min: " + String(minHeap) + " B");
            
            if (freeHeap < 20000) {
                log_msg(LOG_LEVEL_WARN, "!!! LOW MEMORY WARNING !!!");
                buzzerBeep(2, 100, 100);
            }
        }
        
        // Sprawdź watchdogi innych zadań co 10 sekund
        if (now - lastWatchdogCheck > 10000) {
            lastWatchdogCheck = now;
            for (int i = 0; i < 6; i++) {
                checkTaskWatchdog(i);
                if (taskWatchdogs[i].timeoutDetected) {
                    log_msg(LOG_LEVEL_ERROR, String(taskWatchdogs[i].taskName) + " task is hung!");
                }
            }
        }
        
        // Log statystyk procesu co 5 minut
        if (now - lastStatsLog > 300000) {
            lastStatsLog = now;
            
            if (state_lock()) {
                ProcessStats stats = g_processStats;
                state_unlock();
                
                if (stats.totalRunTime > 0) {
                    unsigned long runHours = stats.totalRunTime / 3600000;
                    unsigned long runMins = (stats.totalRunTime % 3600000) / 60000;
                    unsigned long heatPercent = (stats.activeHeatingTime * 100) / stats.totalRunTime;
                    
                    log_msg(LOG_LEVEL_INFO, "[STATS] Runtime: " + String(runHours) + "h " + String(runMins) + "m");
                    log_msg(LOG_LEVEL_INFO, "[STATS] Heating: " + String(heatPercent) + "%, Avg temp: " + 
                            String(stats.avgTemp, 1) + "°C");
                    log_msg(LOG_LEVEL_INFO, "[STATS] Steps: " + String(stats.stepChanges) + 
                            ", Pauses: " + String(stats.pauseCount));
                }
            }
            
            // WiFi stats
            if (wifi_is_connected()) {
                WiFiStats wifiStats = wifi_get_stats();
                unsigned long upHours = wifiStats.totalUptime / 3600000;
                unsigned long downHours = wifiStats.totalDowntime / 3600000;
                log_msg(LOG_LEVEL_INFO, "[WiFi] Up: " + String(upHours) + "h, Down: " + 
                        String(downHours) + "h, Disconnects: " + String(wifiStats.disconnectCount));
            }
        }
        
        checkTaskWatchdog(taskIndex);
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void tasks_create_all() {
    watchdog_init();
    
    // Core 1: Zadania krytyczne
    xTaskCreatePinnedToCore(taskControl, "Control", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(taskSensors, "Sensors", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskUI, "UI", 4096, NULL, 2, NULL, 1);
    
    // Core 0: Zadania sieciowe i monitoring
    xTaskCreatePinnedToCore(taskWeb, "Web", 10240, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskWiFi, "WiFi", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskMonitor, "Monitor", 4096, NULL, 1, NULL, 0);
    
    log_msg(LOG_LEVEL_INFO, "All tasks created successfully");
}

// Nowa funkcja: status watchdog
String getTaskWatchdogStatus() {
    String status = "Task Watchdogs:\n";
    for (int i = 0; i < 6; i++) {
        TickType_t now = xTaskGetTickCount();
        unsigned long age = (now - taskWatchdogs[i].lastReset) * portTICK_PERIOD_MS;
        status += String(taskWatchdogs[i].taskName) + ": ";
        status += taskWatchdogs[i].timeoutDetected ? "TIMEOUT" : "OK";
        status += " (age: " + String(age) + "ms)\n";
    }
    return status;
}
