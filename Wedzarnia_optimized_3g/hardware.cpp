// hardware.cpp - Zmodernizowana wersja z diagnostyką
#include "hardware.h"
#include "config.h"
#include "state.h"
#include "outputs.h"
#include "wifimanager.h"
#include <SD.h>
#include <nvs_flash.h>
#include <WiFi.h>

// Plik logów
static File logFile;

void hardware_init_pins() {
    // Wyjścia
    pinMode(PIN_SSR1, OUTPUT);
    pinMode(PIN_SSR2, OUTPUT);
    pinMode(PIN_SSR3, OUTPUT);
    pinMode(PIN_FAN, OUTPUT);
    pinMode(PIN_SMOKE_FAN, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    
    // Wejścia
    pinMode(PIN_DOOR, INPUT_PULLUP);
    pinMode(PIN_BTN_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
    pinMode(PIN_BTN_ENTER, INPUT_PULLUP);
    pinMode(PIN_BTN_EXIT, INPUT_PULLUP);
    
    log_msg(LOG_LEVEL_INFO, "GPIO pins initialized");
}

void hardware_init_ledc() {
    bool success = true;
    
    if (!ledcAttach(PIN_SSR1, LEDC_FREQ, LEDC_RESOLUTION)) {
        log_msg(LOG_LEVEL_ERROR, "LEDC SSR1 attach failed!");
        success = false;
    }
    if (!ledcAttach(PIN_SSR2, LEDC_FREQ, LEDC_RESOLUTION)) {
        log_msg(LOG_LEVEL_ERROR, "LEDC SSR2 attach failed!");
        success = false;
    }
    if (!ledcAttach(PIN_SSR3, LEDC_FREQ, LEDC_RESOLUTION)) {
        log_msg(LOG_LEVEL_ERROR, "LEDC SSR3 attach failed!");
        success = false;
    }
    if (!ledcAttach(PIN_SMOKE_FAN, LEDC_FREQ, LEDC_RESOLUTION)) {
        log_msg(LOG_LEVEL_ERROR, "LEDC SMOKE attach failed!");
        success = false;
    }
    
    allOutputsOff();
    
    if (success) {
        log_msg(LOG_LEVEL_INFO, "LEDC/PWM initialized");
    }
}

void hardware_init_sensors() {
    sensors.begin();
    sensors.setWaitForConversion(false);
    sensors.setResolution(12);
    
    int deviceCount = sensors.getDeviceCount();
    log_msg(LOG_LEVEL_INFO, "Found " + String(deviceCount) + " DS18B20 sensor(s)");
    
    if (deviceCount == 0) {
        log_msg(LOG_LEVEL_WARN, "No temperature sensors found!");
    }
}

void hardware_init_display() {
    display.initR(INITR_BLACKTAB);
    display.setRotation(0);
    display.fillScreen(ST77XX_BLACK);
    display.setCursor(10, 20);
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(2);
    display.println("WEDZARNIA");
    display.setTextSize(1);
    display.println("\n   v3.3 by Wojtek");
    display.println("\n   Inicjalizacja...");
    delay(2000);
    log_msg(LOG_LEVEL_INFO, "Display initialized");
}

void hardware_init_sd() {
    // Opóźnienie dla stabilizacji karty SD
    delay(200);
    
    if (!SD.begin(PIN_SD_CS)) {
        if (state_lock()) {
            g_errorProfile = true;
            state_unlock();
        }
        log_msg(LOG_LEVEL_ERROR, "SD card error - manual mode only");
        buzzerBeep(3, 200, 200);
    } else {
        // Sprawdź czy katalog /profiles istnieje
        if (!SD.exists("/profiles")) {
            log_msg(LOG_LEVEL_WARN, "/profiles directory not found on SD card");
        } else {
            log_msg(LOG_LEVEL_INFO, "SD card OK");
        }
        
        // Inicjalizuj system logowania
        initLoggingSystem();
    }
}

void nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        log_msg(LOG_LEVEL_INFO, "Erasing NVS flash...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    log_msg(LOG_LEVEL_INFO, "NVS initialized");
}

void hardware_init_wifi() {
    wifi_init();
}

// Nowe funkcje diagnostyczne

void initLoggingSystem() {
    if (!SD.exists("/logs")) {
        if (SD.mkdir("/logs")) {
            log_msg(LOG_LEVEL_INFO, "Created /logs directory");
        } else {
            log_msg(LOG_LEVEL_ERROR, "Failed to create /logs directory");
            return;
        }
    }
    
    // Sprawdź liczbę istniejących plików logów
    File logDir = SD.open("/logs");
    int fileCount = 0;
    while (File entry = logDir.openNextFile()) {
        if (!entry.isDirectory()) fileCount++;
        entry.close();
    }
    logDir.close();
    
    // Usuń najstarsze logi jeśli jest ich za dużo
    if (fileCount > 10) {
        deleteOldestLog("/logs");
    }
    
    // Utwórz nowy plik logów z timestampem
    char filename[32];
    snprintf(filename, sizeof(filename), "/logs/wedzarnia_%lu.log", millis() / 1000);
    File logFile = SD.open(filename, FILE_WRITE);
    
    if (logFile) {
        logFile.println("=== WEDZARNIA LOG START ===");
        logFile.printf("Timestamp: %lu\n", millis() / 1000);
        logFile.printf("Free heap: %d\n", ESP.getFreeHeap());
        logFile.close();
        log_msg(LOG_LEVEL_INFO, "Log file created: " + String(filename));
    } else {
        log_msg(LOG_LEVEL_ERROR, "Failed to create log file");
    }
}

// DODANA IMPLEMENTACJA FUNKCJI
void deleteOldestLog(const char* dirPath) {
    if (!SD.exists(dirPath)) return;
    
    File dir = SD.open(dirPath);
    String oldestFile;
    unsigned long oldestTime = ULONG_MAX;
    
    while (File entry = dir.openNextFile()) {
        if (!entry.isDirectory()) {
            String fileName = entry.name();
            
            // Szukaj plików z timestampem w nazwie
            if (fileName.startsWith("/logs/wedzarnia_") && fileName.endsWith(".log")) {
                // Wyodrębnij timestamp z nazwy
                int start = fileName.indexOf('_') + 1;
                int end = fileName.lastIndexOf('.');
                if (start > 0 && end > start) {
                    String timeStr = fileName.substring(start, end);
                    unsigned long fileTime = timeStr.toInt();
                    
                    if (fileTime < oldestTime) {
                        oldestTime = fileTime;
                        oldestFile = fileName;
                    }
                }
            }
        }
        entry.close();
    }
    dir.close();
    
    if (oldestFile.length() > 0 && oldestTime != ULONG_MAX) {
        if (SD.remove(oldestFile.c_str())) {
            log_msg(LOG_LEVEL_INFO, "Deleted oldest log: " + oldestFile);
        } else {
            log_msg(LOG_LEVEL_ERROR, "Failed to delete: " + oldestFile);
        }
    }
}

void logToFile(const String& message) {
    if (logFile = SD.open("/logs/latest.log", FILE_APPEND)) {
        logFile.printf("[%lu] %s\n", millis() / 1000, message.c_str());
        logFile.close();
    }
}

void runStartupSelfTest() {
    log_msg(LOG_LEVEL_INFO, "Running startup self-test...");
    
    // Rozszerzony test przycisków z informacjami
    log_msg(LOG_LEVEL_INFO, "=== BUTTONS TEST ===");
    Serial.println("Testing all buttons (pull-up should show HIGH):");
    
    for (int i = 0; i < 10; i++) {
        Serial.printf("[%d] UP:%d DOWN:%d ENTER:%d EXIT:%d DOOR:%d\n", 
            i, 
            digitalRead(PIN_BTN_UP),
            digitalRead(PIN_BTN_DOWN),
            digitalRead(PIN_BTN_ENTER),
            digitalRead(PIN_BTN_EXIT),
            digitalRead(PIN_DOOR));
        delay(100);
    }
    
    // Test reaktywności przycisków
    log_msg(LOG_LEVEL_INFO, "Testing button responsiveness...");
    buzzerBeep(1, 100, 0);
    
    // Test przycisku UP
    log_msg(LOG_LEVEL_INFO, "Please press UP button within 5 seconds...");
    unsigned long start = millis();
    bool upPressed = false;
    while (millis() - start < 5000) {
        if (digitalRead(PIN_BTN_UP) == LOW) {
            log_msg(LOG_LEVEL_INFO, "UP button OK! (LOW when pressed)");
            buzzerBeep(1, 50, 0);
            upPressed = true;
            break;
        }
        delay(10);
    }
    if (!upPressed) {
        log_msg(LOG_LEVEL_WARN, "UP button not pressed - check connection");
    }
    
    // Test przycisku DOWN
    delay(1000);
    log_msg(LOG_LEVEL_INFO, "Please press DOWN button within 5 seconds...");
    start = millis();
    bool downPressed = false;
    while (millis() - start < 5000) {
        if (digitalRead(PIN_BTN_DOWN) == LOW) {
            log_msg(LOG_LEVEL_INFO, "DOWN button OK! (LOW when pressed)");
            buzzerBeep(1, 50, 0);
            downPressed = true;
            break;
        }
        delay(10);
    }
    if (!downPressed) {
        log_msg(LOG_LEVEL_WARN, "DOWN button not pressed - check connection");
    }
    
    // Test przycisku ENTER
    delay(1000);
    log_msg(LOG_LEVEL_INFO, "Please press ENTER button within 5 seconds...");
    start = millis();
    bool enterPressed = false;
    while (millis() - start < 5000) {
        if (digitalRead(PIN_BTN_ENTER) == LOW) {
            log_msg(LOG_LEVEL_INFO, "ENTER button OK! (LOW when pressed)");
            buzzerBeep(1, 50, 0);
            enterPressed = true;
            break;
        }
        delay(10);
    }
    if (!enterPressed) {
        log_msg(LOG_LEVEL_WARN, "ENTER button not pressed - check connection");
    }
    
    // Test przycisku EXIT
    delay(1000);
    log_msg(LOG_LEVEL_INFO, "Please press EXIT button within 5 seconds...");
    start = millis();
    bool exitPressed = false;
    while (millis() - start < 5000) {
        if (digitalRead(PIN_BTN_EXIT) == LOW) {
            log_msg(LOG_LEVEL_INFO, "EXIT button OK! (LOW when pressed)");
            buzzerBeep(1, 50, 0);
            exitPressed = true;
            break;
        }
        delay(10);
    }
    if (!exitPressed) {
        log_msg(LOG_LEVEL_WARN, "EXIT button not pressed - check connection");
    }
    
    // Podsumowanie testu przycisków
    log_msg(LOG_LEVEL_INFO, "=== BUTTON TEST SUMMARY ===");
    log_msg(LOG_LEVEL_INFO, "UP: " + String(upPressed ? "OK" : "FAILED"));
    log_msg(LOG_LEVEL_INFO, "DOWN: " + String(downPressed ? "OK" : "FAILED"));
    log_msg(LOG_LEVEL_INFO, "ENTER: " + String(enterPressed ? "OK" : "FAILED"));
    log_msg(LOG_LEVEL_INFO, "EXIT: " + String(exitPressed ? "OK" : "FAILED"));
    
    // Test czujników
    log_msg(LOG_LEVEL_INFO, "Testing temperature sensors...");
    sensors.requestTemperatures();
    delay(1000);
    double temp1 = sensors.getTempCByIndex(0);
    double temp2 = sensors.getTempCByIndex(1);
    
    if (temp1 == DEVICE_DISCONNECTED_C) {
        log_msg(LOG_LEVEL_ERROR, "Sensor 1 failed!");
    } else {
        log_msg(LOG_LEVEL_INFO, "Sensor 1: " + String(temp1, 1) + "°C");
    }
    
    if (temp2 == DEVICE_DISCONNECTED_C) {
        log_msg(LOG_LEVEL_WARN, "Sensor 2 not connected");
    } else {
        log_msg(LOG_LEVEL_INFO, "Sensor 2: " + String(temp2, 1) + "°C");
    }
    
    // Test wyjść z krótkimi impulsami
    log_msg(LOG_LEVEL_INFO, "Testing outputs...");
    testOutput(PIN_SSR1, "Heater 1");
    testOutput(PIN_SSR2, "Heater 2");
    testOutput(PIN_SSR3, "Heater 3");
    testOutput(PIN_FAN, "Fan");
    testOutput(PIN_BUZZER, "Buzzer");
    
    // Test drzwi
    bool doorState = digitalRead(PIN_DOOR);
    log_msg(LOG_LEVEL_INFO, "Door sensor: " + String(doorState ? "OPEN" : "CLOSED"));
    
    // Finalne podsumowanie
    log_msg(LOG_LEVEL_INFO, "=== STARTUP SELF-TEST COMPLETED ===");
    log_msg(LOG_LEVEL_INFO, "Buttons: " + 
        String(upPressed && downPressed && enterPressed && exitPressed ? "ALL OK" : "SOME FAILED"));
    log_msg(LOG_LEVEL_INFO, "Sensors: " + String(temp1 != DEVICE_DISCONNECTED_C ? "OK" : "FAILED"));
    log_msg(LOG_LEVEL_INFO, "Outputs: TESTED");
    log_msg(LOG_LEVEL_INFO, "Door: " + String(doorState ? "OPEN" : "CLOSED"));
    
    // Dźwięk końcowy
    if (upPressed && downPressed && enterPressed && exitPressed && temp1 != DEVICE_DISCONNECTED_C) {
        buzzerBeep(3, 100, 50); // Sukces
    } else {
        buzzerBeep(5, 200, 100); // Błąd/warning
    }
}

void testOutput(int pin, const char* name) {
    digitalWrite(pin, HIGH);
    delay(100);
    digitalWrite(pin, LOW);
    log_msg(LOG_LEVEL_INFO, String(name) + " test: OK");
}

void testButton(int pin, const char* name) {
    bool state = digitalRead(pin);
    log_msg(LOG_LEVEL_INFO, String(name) + ": " + String(state ? "HIGH" : "LOW"));
}

void enterLowPowerMode() {
    log_msg(LOG_LEVEL_INFO, "Entering low power mode");
    
    // Wyłącz niepotrzebne peryferia
    // display.sleep(true);
    WiFi.mode(WIFI_OFF);
    SD.end();
    
    // Ustaw czujnik drzwi do wybudzenia
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_DOOR, LOW);
    
    // Ustaw przycisk ENTER jako dodatkowy wakeup
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_BTN_ENTER, ESP_EXT1_WAKEUP_ANY_HIGH);
    
    log_msg(LOG_LEVEL_INFO, "Entering deep sleep...");
    delay(100);
    esp_deep_sleep_start();
}

bool shouldEnterLowPower() {
    static unsigned long lastActivity = millis();
    
    state_lock();
    bool isIdle = (g_currentState == ProcessState::IDLE);
    state_unlock();
    
    if (isIdle) {
        unsigned long idleTime = millis() - lastActivity;
        if (idleTime > LOW_POWER_TIMEOUT) {
            return true;
        }
    } else {
        lastActivity = millis();
    }
    
    return false;
}