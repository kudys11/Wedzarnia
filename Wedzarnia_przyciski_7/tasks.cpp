// tasks.cpp
#include "tasks.h"
#include "config.h"
#include "state.h"
#include "process.h"
#include "sensors.h"
#include "ui.h"
#include "outputs.h"
#include "web_server.h"
#include <esp_task_wdt.h>

static void watchdog_init() {
    esp_task_wdt_config_t wdt_config = { 
        .timeout_ms = WDT_TIMEOUT * 1000, 
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, 
        .trigger_panic = true 
    };
    esp_task_wdt_reconfigure(&wdt_config);
}

void taskControl(void* pv) {
    esp_task_wdt_add(NULL);
    for (;;) {
        esp_task_wdt_reset();
        process_run_control_logic();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void taskSensors(void* pv) {
    esp_task_wdt_add(NULL);
    for (;;) {
        esp_task_wdt_reset();
        requestTemperature();
        readTemperature();
        checkDoor();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void taskUI(void* pv) {
    esp_task_wdt_add(NULL);
    for (;;) {
        esp_task_wdt_reset();
        ui_handle_buttons();
        handleBuzzer();
        ui_update_display();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// POPRAWKA: Dodany WDT
void taskWeb(void* pv) {
    esp_task_wdt_add(NULL);  // Dodane!
    for (;;) {
        esp_task_wdt_reset();  // Dodane!
        web_server_handle_client();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void taskMonitor(void* pv) {
    unsigned long lastHeapLog = 0;
    for (;;) {
        if (millis() - lastHeapLog > 60000) {
            lastHeapLog = millis();
            uint32_t freeHeap = ESP.getFreeHeap();
            uint32_t minHeap = ESP.getMinFreeHeap();
            Serial.printf("[HEAP] Free: %u B, Min: %u B\n", freeHeap, minHeap);
            if (freeHeap < 20000) {
                Serial.println("!!! LOW MEMORY WARNING !!!");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void tasks_create_all() {
    watchdog_init();
    // Zwiększony stos dla taskWeb (8192 -> 10240)
    xTaskCreatePinnedToCore(taskControl, "Control", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(taskSensors, "Sensors", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskUI, "UI", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskWeb, "Web", 10240, NULL, 1, NULL, 0);  // Zwiększony!
    xTaskCreatePinnedToCore(taskMonitor, "Monitor", 2048, NULL, 1, NULL, 0);
}
