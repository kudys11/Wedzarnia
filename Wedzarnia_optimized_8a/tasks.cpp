// tasks.cpp - [FIX] snprintf w logach, zwiększone stosy
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
    LOG_FMT(LOG_LEVEL_INFO, "Watchdog initialized (%ds timeout)", WDT_TIMEOUT);
}

static void checkTaskWatchdog(int taskIndex) {
    TaskWatchdog& wd = taskWatchdogs[taskIndex];
    TickType_t now = xTaskGetTickCount();

    if (now - wd.lastReset > pdMS_TO_TICKS(TASK_WATCHDOG_TIMEOUT)) {
        if (!wd.timeoutDetected) {
            wd.timeoutDetected = true;
            LOG_FMT(LOG_LEVEL_ERROR, "%s task timeout detected!", wd.taskName);

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

            LOG_FMT(LOG_LEVEL_INFO, "[HEAP] Free: %u B, Min: %u B", freeHeap, minHeap);

            if (freeHeap < HEAP_WARNING_THRESHOLD) {
                log_msg(LOG_LEVEL_WARN, "!!! LOW MEMORY WARNING !!!");
                buzzerBeep(2, 100, 100);
            }
        }

        // Sprawdź watchdogi co 10 sekund
        if (now - lastWatchdogCheck > 10000) {
            lastWatchdogCheck = now;
            for (int i = 0; i < 6; i++) {
                checkTaskWatchdog(i);
                if (taskWatchdogs[i].timeoutDetected) {
                    LOG_FMT(LOG_LEVEL_ERROR, "%s task is hung!", taskWatchdogs[i].taskName);
                }
            }
        }

        // Log statystyk co 5 minut
        if (now - lastStatsLog > 300000) {
            lastStatsLog = now;

            if (state_lock()) {
                ProcessStats stats = g_processStats;
                state_unlock();

                if (stats.totalRunTime > 0) {
                    unsigned long runHours = stats.totalRunTime / 3600000;
                    unsigned long runMins = (stats.totalRunTime % 3600000) / 60000;
                    unsigned long heatPercent = (stats.activeHeatingTime * 100) / stats.totalRunTime;

                    LOG_FMT(LOG_LEVEL_INFO, "[STATS] Runtime: %luh %lum", runHours, runMins);
                    LOG_FMT(LOG_LEVEL_INFO, "[STATS] Heating: %lu%%, Avg temp: %.1f C",
                            heatPercent, stats.avgTemp);
                    LOG_FMT(LOG_LEVEL_INFO, "[STATS] Steps: %d, Pauses: %d",
                            stats.stepChanges, stats.pauseCount);
                }
            }

            if (wifi_is_connected()) {
                WiFiStats wifiStats = wifi_get_stats();
                unsigned long upHours = wifiStats.totalUptime / 3600000;
                unsigned long downHours = wifiStats.totalDowntime / 3600000;
                LOG_FMT(LOG_LEVEL_INFO, "[WiFi] Up: %luh, Down: %luh, Disconnects: %d",
                        upHours, downHours, wifiStats.disconnectCount);
            }
        }

        checkTaskWatchdog(taskIndex);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void tasks_create_all() {
    watchdog_init();

    // Core 1: Zadania krytyczne
    // [FIX] Zwiększony stos Sensors z 4096 do 5120 (DS18B20 + NVS operacje)
    xTaskCreatePinnedToCore(taskControl, "Control", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(taskSensors, "Sensors", 5120, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskUI, "UI", 4096, NULL, 2, NULL, 1);

    // Core 0: Zadania sieciowe i monitoring
    xTaskCreatePinnedToCore(taskWeb, "Web", 10240, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskWiFi, "WiFi", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskMonitor, "Monitor", 4096, NULL, 1, NULL, 0);

    log_msg(LOG_LEVEL_INFO, "All tasks created successfully");
}

// [FIX] getTaskWatchdogStatus - snprintf zamiast String
String getTaskWatchdogStatus() {
    char buffer[384];
    int offset = 0;
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Task Watchdogs:\n");

    for (int i = 0; i < 6; i++) {
        TickType_t now = xTaskGetTickCount();
        unsigned long age = (now - taskWatchdogs[i].lastReset) * portTICK_PERIOD_MS;
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                          "%s: %s (age: %lums)\n",
                          taskWatchdogs[i].taskName,
                          taskWatchdogs[i].timeoutDetected ? "TIMEOUT" : "OK",
                          age);
    }
    return String(buffer);
}
