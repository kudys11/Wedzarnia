// hardware.cpp - Zmodernizowana wersja z retry dla SD i uproszczonymi testami
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
    
    // Wejścia z pull-up
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
    display.println("\n   v4.0 by Wojtek");
    display.println("\n   Inicjalizacja...");
    delay(1500); // Skrócone z 2000ms
    log_msg(LOG_LEVEL_INFO, "Display initialized");
}

void hardware_init_sd() {
    const int MAX_RETRIES = 3;
    const unsigned long RETRY_DELAY_MS = 300;
    const unsigned long INIT_TIMEOUT_MS = 2000;
    
    log_msg(LOG_LEVEL_INFO, "Initializing SD card...");
    
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        log_msg(LOG_LEVEL_INFO, "SD init attempt " + String(attempt) + "/" + String(MAX_RETRIES));
        
        unsigned long startTime = millis();
        bool initSuccess = false;
        
        // Próba inicjalizacji z timeoutem
        while (millis() - startTime < INIT_TIMEOUT_MS) {
            if (SD.begin(PIN_SD_CS)) {
                initSuccess = true;
                break;
            }
            delay(50);
        }
        
        if (initSuccess) {
            // Sprawdź typ karty
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_NONE) {
                log_msg(LOG_LEVEL_WARN, "SD card detected but type is NONE");
                SD.end();
                delay(RETRY_DELAY_MS);
                continue;
            }
            
            // Sprawdź czy można odczytać kartę
            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            log_msg(LOG_LEVEL_INFO, "SD card OK: " + String(cardSize) + " MB");
            
            // Sprawdź/utwórz katalogi
            if (!SD.exists("/profiles")) {
                if (SD.mkdir("/profiles")) {
                    log_msg(LOG_LEVEL_INFO, "Created /profiles directory");
                } else {
                    log_msg(LOG_LEVEL_WARN, "Failed to create /profiles directory");
                }
            } else {
                log_msg(LOG_LEVEL_INFO, "/profiles directory exists");
            }
            
            // Inicjalizuj system logowania
            initLoggingSystem();
            
            return; // Sukces!
        }
        
        // Niepowodzenie - poczekaj przed kolejną próbą
        if (attempt < MAX_RETRIES) {
            log_msg(LOG_LEVEL_WARN, "SD init failed, retrying in " + String(RETRY_DELAY_MS) + "ms...");
            SD.end();
            delay(RETRY_DELAY_MS);
        }
    }
    
    // Wszystkie próby nieudane
    log_msg(LOG_LEVEL_ERROR, "SD card initialization failed after " + String(MAX_RETRIES) + " attempts");
    log_msg(LOG_LEVEL_ERROR, "System will run in MANUAL MODE ONLY");
    
    if (state_lock()) {
        g_errorProfile = true;
        state_unlock();
    }
    
    buzzerBeep(3, 200, 200);
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
    
    // ===========================================
    // TEST 1: PRZYCISKI - Uproszczona wersja
    // ===========================================
    log_msg(LOG_LEVEL_INFO, "=== BUTTONS TEST (Quick Check) ===");
    
    bool upOk = (digitalRead(PIN_BTN_UP) == HIGH);      // Pull-up -> HIGH gdy nieprzycisniete
    bool downOk = (digitalRead(PIN_BTN_DOWN) == HIGH);
    bool enterOk = (digitalRead(PIN_BTN_ENTER) == HIGH);
    bool exitOk = (digitalRead(PIN_BTN_EXIT) == HIGH);
    bool doorOk = true; // Drzwi mogą być otwarte lub zamknięte
    
    log_msg(LOG_LEVEL_INFO, "UP: " + String(upOk ? "OK (HIGH)" : "PRESSED or ERROR"));
    log_msg(LOG_LEVEL_INFO, "DOWN: " + String(downOk ? "OK (HIGH)" : "PRESSED or ERROR"));
    log_msg(LOG_LEVEL_INFO, "ENTER: " + String(enterOk ? "OK (HIGH)" : "PRESSED or ERROR"));
    log_msg(LOG_LEVEL_INFO, "EXIT: " + String(exitOk ? "OK (HIGH)" : "PRESSED or ERROR"));
    log_msg(LOG_LEVEL_INFO, "DOOR: " + String(digitalRead(PIN_DOOR) ? "OPEN" : "CLOSED"));
    
    bool allButtonsOk = upOk && downOk && enterOk && exitOk;
    
    if (!allButtonsOk) {
        log_msg(LOG_LEVEL_WARN, "WARNING: Some buttons may be stuck or have wiring issues");
        log_msg(LOG_LEVEL_WARN, "If buttons don't respond, check connections");
    } else {
        log_msg(LOG_LEVEL_INFO, "All buttons idle (pull-up HIGH) - OK");
    }
    
    buzzerBeep(1, 100, 0); // Krótki sygnał
    
    // ===========================================
    // TEST 2: CZUJNIKI TEMPERATURY
    // ===========================================
    log_msg(LOG_LEVEL_INFO, "=== TEMPERATURE SENSORS TEST ===");
    
    sensors.requestTemperatures();
    delay(1000);
    
    int sensorCount = sensors.getDeviceCount();
    bool sensor1Ok = false;
    bool sensor2Ok = false;
    
    if (sensorCount >= 1) {
        double temp1 = sensors.getTempCByIndex(0);
        if (temp1 != DEVICE_DISCONNECTED_C && temp1 > -20 && temp1 < 100) {
            log_msg(LOG_LEVEL_INFO, "Sensor 1: " + String(temp1, 1) + "°C - OK");
            sensor1Ok = true;
        } else {
            log_msg(LOG_LEVEL_ERROR, "Sensor 1: FAILED or invalid reading");
        }
    }
    
    if (sensorCount >= 2) {
        double temp2 = sensors.getTempCByIndex(1);
        if (temp2 != DEVICE_DISCONNECTED_C && temp2 > -20 && temp2 < 100) {
            log_msg(LOG_LEVEL_INFO, "Sensor 2: " + String(temp2, 1) + "°C - OK");
            sensor2Ok = true;
        } else {
            log_msg(LOG_LEVEL_WARN, "Sensor 2: Not connected or invalid");
        }
    } else {
        log_msg(LOG_LEVEL_WARN, "Only 1 sensor detected (minimum for basic operation)");
    }
    
    // ===========================================
    // TEST 3: WYJŚCIA (Krótki test bez długich opóźnień)
    // ===========================================
    log_msg(LOG_LEVEL_INFO, "=== OUTPUT TEST ===");
    
    // Bardzo krótkie impulsy testowe
    testOutput(PIN_SSR1, "Heater 1");
    delay(50);
    testOutput(PIN_SSR2, "Heater 2");
    delay(50);
    testOutput(PIN_SSR3, "Heater 3");
    delay(50);
    testOutput(PIN_FAN, "Fan");
    delay(50);
    
    // Wszystkie wyjścia OFF po teście
    allOutputsOff();
    
    // ===========================================
    // PODSUMOWANIE
    // ===========================================
    log_msg(LOG_LEVEL_INFO, "=== STARTUP SELF-TEST SUMMARY ===");
    log_msg(LOG_LEVEL_INFO, "Buttons: " + String(allButtonsOk ? "ALL OK" : "CHECK CONNECTIONS"));
    log_msg(LOG_LEVEL_INFO, "Sensors: " + String(sensor1Ok ? "OK" : "FAILED") + 
            " (" + String(sensorCount) + " detected)");
    log_msg(LOG_LEVEL_INFO, "Outputs: TESTED");
    log_msg(LOG_LEVEL_INFO, "Door: " + String(digitalRead(PIN_DOOR) ? "OPEN" : "CLOSED"));
    
    // Sygnał końcowy
    if (allButtonsOk && sensor1Ok) {
        buzzerBeep(2, 100, 50); // Sukces - 2 krótkie beepy
    } else {
        buzzerBeep(4, 150, 100); // Ostrzeżenie - 4 beepy
    }
    
    log_msg(LOG_LEVEL_INFO, "Self-test completed");
}

void testOutput(int pin, const char* name) {
    digitalWrite(pin, HIGH);
    delay(50); // Bardzo krótki puls - 50ms zamiast 100ms
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
