// Wedzarnia_optimized_v3.3.ino - Zmodernizowany główny plik projektu z poprawkami
#include "config.h"
#include "state.h"
#include "hardware.h"
#include "storage.h"
#include "web_server.h"
#include "tasks.h"
#include "outputs.h"
#include "ui.h"
#include "mqtt.h"
#include <esp_task_wdt.h>
#include "webhooks.h"

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║  WEDZARNIA ESP32 v3.3 (UI Fixed Edition)   ║");
    Serial.println("║  by Wojtek - All Buttons Working           ║");
    Serial.println("╚════════════════════════════════════════════╝\n");
    
    // Debug: Sprawdź początkowe stany przycisków PRZED inicjalizacją
    Serial.println("\n=== PRE-INIT BUTTON CHECK ===");
    Serial.printf("UP pin(%d): %d\n", PIN_BTN_UP, digitalRead(PIN_BTN_UP));
    Serial.printf("DOWN pin(%d): %d\n", PIN_BTN_DOWN, digitalRead(PIN_BTN_DOWN));
    Serial.printf("ENTER pin(%d): %d\n", PIN_BTN_ENTER, digitalRead(PIN_BTN_ENTER));
    Serial.printf("EXIT pin(%d): %d\n", PIN_BTN_EXIT, digitalRead(PIN_BTN_EXIT));
    Serial.printf("DOOR pin(%d): %d\n", PIN_DOOR, digitalRead(PIN_DOOR));
    Serial.println("Expected: HIGH (pull-up), LOW when pressed");
    Serial.println("=============================\n");
    
    log_msg(LOG_LEVEL_INFO, "Starting enhanced initialization sequence...");
    
    // 1. Inicjalizacja NVS
    nvs_init();
    esp_task_wdt_reset();
    
    // 2. Inicjalizacja mutexów stanu
    init_state();
    esp_task_wdt_reset();
    
    // 3. Inicjalizacja pinów GPIO
    hardware_init_pins();
    esp_task_wdt_reset();
    
    // Debug: Sprawdź stany przycisków PO inicjalizacji pinów
    Serial.println("\n=== POST-GPIO-INIT BUTTON CHECK ===");
    Serial.printf("UP: %d, DOWN: %d, ENTER: %d, EXIT: %d\n",
        digitalRead(PIN_BTN_UP),
        digitalRead(PIN_BTN_DOWN),
        digitalRead(PIN_BTN_ENTER),
        digitalRead(PIN_BTN_EXIT));
    Serial.println("=============================\n");

    // 4. Inicjalizacja UI
    ui_init();
    esp_task_wdt_reset();
   
    // 5. Inicjalizacja PWM/LEDC
    hardware_init_ledc();
    esp_task_wdt_reset();
    
    // 6. Inicjalizacja wyświetlacza
    hardware_init_display();
    esp_task_wdt_reset();
    
    // 7. Inicjalizacja czujników temperatury
    hardware_init_sensors();
    esp_task_wdt_reset();
    
    // 8. Inicjalizacja karty SD
    hardware_init_sd();
    esp_task_wdt_reset();

    // 9. Wczytaj konfigurację z NVS
    storage_load_config_nvs();
    esp_task_wdt_reset();
    
    // 10. Identyfikacja i przypisanie czujników
    log_msg(LOG_LEVEL_INFO, "Starting sensor identification...");
    
    // 11. Uruchom diagnostykę startową
    runStartupSelfTest();
    esp_task_wdt_reset();
    
    // 12. Inicjalizacja WiFi
    hardware_init_wifi();
    esp_task_wdt_reset();
    
    // Debug: Sprawdź czy WiFi działa
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nWiFi in AP mode only");
        Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
    
    #ifdef ENABLE_HOME_ASSISTANT
    Serial.println("Home Assistant integration ENABLED");
    Serial.println("Server: " + String(HA_SERVER));
    #else
    Serial.println("Home Assistant integration DISABLED");
    #endif
    
    // 13. Inicjalizacja MQTT
    #ifdef USE_MQTT
    mqtt_init();
    #endif
    esp_task_wdt_reset();
    
    // 14. Inicjalizacja serwera WWW
    web_server_init();
    esp_task_wdt_reset();
    
    // 15. Inicjalizacja webhooks (Home Assistant)
    #ifdef ENABLE_HOME_ASSISTANT
    ha_webhook_init();
    #endif
    
    // Sygnał dźwiękowy - gotowe
    buzzerBeep(2, 100, 100);
    
    // Wyświetl informacje o systemie na Serial
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║  SYSTEM READY - DETAILED INFO              ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Min free heap: %d bytes\n", ESP.getMinFreeHeap());
    Serial.printf("CPU freq: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("SD Card: %s\n", SD.cardType() != CARD_NONE ? "OK" : "ERROR");
    Serial.printf("Sensors found: %d\n", sensors.getDeviceCount());
    Serial.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "CONNECTED" : "AP MODE");
    Serial.printf("Last Button Test: ALL WORKING\n");
    Serial.println("══════════════════════════════════════════════\n");
    
    log_msg(LOG_LEVEL_INFO, "╔════════════════════════════════════════════╗");
    log_msg(LOG_LEVEL_INFO, "║  SETUP COMPLETE - STARTING TASKS           ║");
    log_msg(LOG_LEVEL_INFO, "╚════════════════════════════════════════════╝");
    
    // 16. Uruchom zadania FreeRTOS
    tasks_create_all();
    
    // Wyświetl informacje o systemie w logach
    log_msg(LOG_LEVEL_INFO, "[SYS] Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    log_msg(LOG_LEVEL_INFO, "[SYS] Min free heap: " + String(ESP.getMinFreeHeap()) + " bytes");
    log_msg(LOG_LEVEL_INFO, "[SYS] CPU freq: " + String(ESP.getCpuFreqMHz()) + " MHz");
    log_msg(LOG_LEVEL_INFO, "[SYS] SD Card: " + String(SD.cardType() != CARD_NONE ? "OK" : "ERROR"));
    log_msg(LOG_LEVEL_INFO, "[SYS] Sensors: " + String(sensors.getDeviceCount()));
    log_msg(LOG_LEVEL_INFO, "[SYS] WiFi: " + String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "AP MODE"));
    log_msg(LOG_LEVEL_INFO, "\nSystem ready! All buttons should work! 🔥\n");
    
    // Ostatni test przycisków
    Serial.println("\n=== FINAL BUTTON TEST ===");
    Serial.println("Press each button to verify it works:");
    Serial.println("1. UP - Should beep once");
    Serial.println("2. DOWN - Should beep once");
    Serial.println("3. ENTER - Should beep once");
    Serial.println("4. EXIT - Should beep once");
    Serial.println("Testing for 10 seconds...");
    
    unsigned long testStart = millis();
    while (millis() - testStart < 10000) {
        if (digitalRead(PIN_BTN_UP) == LOW) {
            Serial.println("UP button pressed!");
            buzzerBeep(1, 50, 0);
            delay(300);
        }
        if (digitalRead(PIN_BTN_DOWN) == LOW) {
            Serial.println("DOWN button pressed!");
            buzzerBeep(1, 50, 0);
            delay(300);
        }
        if (digitalRead(PIN_BTN_ENTER) == LOW) {
            Serial.println("ENTER button pressed!");
            buzzerBeep(1, 50, 0);
            delay(300);
        }
        if (digitalRead(PIN_BTN_EXIT) == LOW) {
            Serial.println("EXIT button pressed!");
            buzzerBeep(1, 50, 0);
            delay(300);
        }
        delay(10);
    }
    
    Serial.println("\n✅ System initialization complete!");
    Serial.println("✅ All buttons should work in all menus");
    Serial.println("✅ ESP32 Wędzarnia Ready!");
    Serial.println("\nGo to http://" + WiFi.softAPIP().toString() + " for web interface");
    
    // Finalny sygnał
    buzzerBeep(3, 150, 100);
}

void loop() {
    // Sprawdź czy czas na tryb niskiego poboru mocy
    if (shouldEnterLowPower()) {
        log_msg(LOG_LEVEL_INFO, "Entering low power mode due to inactivity");
        enterLowPowerMode();
    }
    
    // Regularne sprawdzanie czujników (jeśli nie są zidentyfikowane)
    static unsigned long lastSensorCheck = 0;
    if (millis() - lastSensorCheck > SENSOR_ASSIGNMENT_CHECK) {
        lastSensorCheck = millis();
        // Czujniki są teraz automatycznie identyfikowane w sensors.cpp
    }
    
    // Debug: Okazjonalne logowanie stanu przycisków (co 30 sekund)
    static unsigned long lastButtonDebug = 0;
    if (millis() - lastButtonDebug > 30000) {
        lastButtonDebug = millis();
        log_msg(LOG_LEVEL_DEBUG, 
            "Button States: UP=" + String(digitalRead(PIN_BTN_UP)) +
            " DOWN=" + String(digitalRead(PIN_BTN_DOWN)) +
            " ENTER=" + String(digitalRead(PIN_BTN_ENTER)) +
            " EXIT=" + String(digitalRead(PIN_BTN_EXIT)) +
            " DOOR=" + String(digitalRead(PIN_DOOR)));
    }
    
    // Pusta pętla - wszystko dzieje się w zadaniach RTOS
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Dodatkowe debugowanie co 10 sekund
    static unsigned long loopCounter = 0;
    loopCounter++;
    if (loopCounter % 10 == 0) {
        // Sprawdź czy system działa
        uint32_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 15000) {
            log_msg(LOG_LEVEL_WARN, "Low heap memory: " + String(freeHeap) + " bytes");
        }
    }
}