// Wedzarnia_optimized_4b_bluetooth.ino - Wersja z poprawioną inicjalizacją

#include "config.h"
#include "state.h"
#include "hardware.h"
#include "storage.h"
#include "web_server.h"
#include "tasks.h" // Upewnij się, że ten include jest obecny
#include "outputs.h"
#include "ui.h"
#include "bluetooth.h"

void setup() {
    Serial.begin(115200);
    delay(100);

    // === KROK 1: Inicjalizuj Watchdog JAKO PIERWSZY ===
    // To jest kluczowe! Konfiguruje watchdog zanim jakakolwiek biblioteka
    // (zwłaszcza Bluetooth) spróbuje go użyć, co zapobiega pętli restartów.
    watchdog_init();

    Serial.println("\n==========================================================");
    Serial.println("      WEDZARNIA ESP32 v3.5 (Bluetooth Edition)      ");
    Serial.println("        by Wojtek - with BT Control                 ");
    Serial.println("==========================================================\n");

    log_msg(APP_LOG_LEVEL_INFO, "Starting initialization sequence...");

    // Sekwencja inicjalizacji
    nvs_init();
    init_state();
    hardware_init_pins();
    ui_init();
    hardware_init_ledc();
    hardware_init_display();
    hardware_init_sensors();
    hardware_init_sd();
    storage_load_config_nvs();
    log_msg(APP_LOG_LEVEL_INFO, "Starting sensor identification...");
    runStartupSelfTest();
    hardware_init_wifi();
    
    #ifdef BLE_SUPPORT
      ble_init();
    #endif

    web_server_init();

    // Sygnalizacja dźwiękowa gotowości
    buzzerBeep(2, 100, 100);

    // Wyświetl informacje o systemie na Serial
    Serial.println("\n==========================================================");
    Serial.println("        SYSTEM READY - DETAILED INFO                  ");
    Serial.println("==========================================================");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("CPU freq: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("SD Card: %s\n", SD.cardType() != CARD_NONE ? "OK" : "ERROR");
    Serial.printf("Sensors found: %d\n", sensors.getDeviceCount());
    Serial.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? ("CONNECTED (" + WiFi.localIP().toString() + ")").c_str() : "AP MODE");
    #ifdef BLE_SUPPORT
      Serial.println("Bluetooth: READY (Wedzarnia_ESP32)");
    #endif
    Serial.println("==========================================================\n");

    log_msg(APP_LOG_LEVEL_INFO, "==========================================================");
    log_msg(APP_LOG_LEVEL_INFO, "      SETUP COMPLETE - STARTING TASKS               ");
    log_msg(APP_LOG_LEVEL_INFO, "==========================================================");

    // === KROK 2: Uruchom wszystkie zadania FreeRTOS ===
    tasks_create_all();

    log_msg(APP_LOG_LEVEL_INFO, "\nSystem ready!\n");

    Serial.println("\nSystem initialization complete!");
    Serial.println("ESP32 Wedzarnia Ready!");

    buzzerBeep(3, 150, 100);
}

void loop() {
    // === KROK 3: Pusta pętla ===
    // W systemie operacyjnym czasu rzeczywistego (RTOS), główna pętla `loop()`
    // powinna pozostać pusta. Cała logika jest obsługiwana przez dedykowane zadania
    // (taskControl, taskUI, taskSensors, itp.), co zapobiega konfliktom i zapewnia
    // stabilne działanie.
    // Usypiamy to zadanie na zawsze, aby nie zużywało zasobów CPU.
    vTaskDelay(portMAX_DELAY);
}
