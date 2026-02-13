// hardware.cpp - [FIX] Naprawiony logToFile, shouldEnterLowPower, snprintf w logach
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
    pinMode(PIN_SSR1, OUTPUT);
    pinMode(PIN_SSR2, OUTPUT);
    pinMode(PIN_SSR3, OUTPUT);
    pinMode(PIN_FAN, OUTPUT);
    pinMode(PIN_SMOKE_FAN, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

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
    LOG_FMT(LOG_LEVEL_INFO, "Found %d DS18B20 sensor(s)", deviceCount);

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
    delay(1500);
    log_msg(LOG_LEVEL_INFO, "Display initialized");
}

void hardware_init_sd() {
    const int MAX_RETRIES = 3;
    const unsigned long RETRY_DELAY_MS = 300;
    const unsigned long INIT_TIMEOUT_MS = 2000;

    log_msg(LOG_LEVEL_INFO, "Initializing SD card...");

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        LOG_FMT(LOG_LEVEL_INFO, "SD init attempt %d/%d", attempt, MAX_RETRIES);

        unsigned long startTime = millis();
        bool initSuccess = false;

        while (millis() - startTime < INIT_TIMEOUT_MS) {
            if (SD.begin(PIN_SD_CS)) {
                initSuccess = true;
                break;
            }
            delay(50);
        }

        if (initSuccess) {
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_NONE) {
                log_msg(LOG_LEVEL_WARN, "SD card detected but type is NONE");
                SD.end();
                delay(RETRY_DELAY_MS);
                continue;
            }

            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            LOG_FMT(LOG_LEVEL_INFO, "SD card OK: %llu MB", cardSize);

            if (!SD.exists("/profiles")) {
                if (SD.mkdir("/profiles")) {
                    log_msg(LOG_LEVEL_INFO, "Created /profiles directory");
                } else {
                    log_msg(LOG_LEVEL_WARN, "Failed to create /profiles directory");
                }
            } else {
                log_msg(LOG_LEVEL_INFO, "/profiles directory exists");
            }

            initLoggingSystem();
            return;
        }

        if (attempt < MAX_RETRIES) {
            LOG_FMT(LOG_LEVEL_WARN, "SD init failed, retrying in %lums...", RETRY_DELAY_MS);
            SD.end();
            delay(RETRY_DELAY_MS);
        }
    }

    LOG_FMT(LOG_LEVEL_ERROR, "SD card init failed after %d attempts", MAX_RETRIES);
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

void initLoggingSystem() {
    if (!SD.exists("/logs")) {
        if (SD.mkdir("/logs")) {
            log_msg(LOG_LEVEL_INFO, "Created /logs directory");
        } else {
            log_msg(LOG_LEVEL_ERROR, "Failed to create /logs directory");
            return;
        }
    }

    File logDir = SD.open("/logs");
    int fileCount = 0;
    while (File entry = logDir.openNextFile()) {
        if (!entry.isDirectory()) fileCount++;
        entry.close();
    }
    logDir.close();

    if (fileCount > 10) {
        deleteOldestLog("/logs");
    }

    char filename[32];
    snprintf(filename, sizeof(filename), "/logs/wedzarnia_%lu.log", millis() / 1000);
    File newLogFile = SD.open(filename, FILE_WRITE);

    if (newLogFile) {
        newLogFile.println("=== WEDZARNIA LOG START ===");
        newLogFile.printf("Timestamp: %lu\n", millis() / 1000);
        newLogFile.printf("Free heap: %d\n", ESP.getFreeHeap());
        newLogFile.close();
        LOG_FMT(LOG_LEVEL_INFO, "Log file created: %s", filename);
    } else {
        log_msg(LOG_LEVEL_ERROR, "Failed to create log file");
    }
}

void deleteOldestLog(const char* dirPath) {
    if (!SD.exists(dirPath)) return;

    File dir = SD.open(dirPath);
    // [FIX] Używamy char[] zamiast String dla nazwy pliku
    char oldestFile[64] = {0};
    unsigned long oldestTime = ULONG_MAX;

    while (File entry = dir.openNextFile()) {
        if (!entry.isDirectory()) {
            const char* fileName = entry.name();

            // Szukaj plików z timestampem w nazwie
            const char* prefix = "/logs/wedzarnia_";
            if (strncmp(fileName, prefix, strlen(prefix)) == 0) {
                const char* timeStart = fileName + strlen(prefix);
                const char* dot = strrchr(fileName, '.');
                if (dot && dot > timeStart) {
                    char timeStr[16];
                    int timeLen = dot - timeStart;
                    if (timeLen > 0 && timeLen < (int)sizeof(timeStr)) {
                        strncpy(timeStr, timeStart, timeLen);
                        timeStr[timeLen] = '\0';
                        unsigned long fileTime = strtoul(timeStr, NULL, 10);

                        if (fileTime < oldestTime) {
                            oldestTime = fileTime;
                            strncpy(oldestFile, fileName, sizeof(oldestFile) - 1);
                        }
                    }
                }
            }
        }
        entry.close();
    }
    dir.close();

    if (oldestFile[0] != '\0' && oldestTime != ULONG_MAX) {
        if (SD.remove(oldestFile)) {
            LOG_FMT(LOG_LEVEL_INFO, "Deleted oldest log: %s", oldestFile);
        } else {
            LOG_FMT(LOG_LEVEL_ERROR, "Failed to delete: %s", oldestFile);
        }
    }
}

void logToFile(const String& message) {
    // [FIX] Naprawiony: było `if (logFile = ...)` (przypisanie zamiast porównania)
    File f = SD.open("/logs/latest.log", FILE_APPEND);
    if (f) {
        f.printf("[%lu] %s\n", millis() / 1000, message.c_str());
        f.close();
    }
}

void runStartupSelfTest() {
    log_msg(LOG_LEVEL_INFO, "Running startup self-test...");

    log_msg(LOG_LEVEL_INFO, "=== BUTTONS TEST (Quick Check) ===");

    bool upOk = (digitalRead(PIN_BTN_UP) == HIGH);
    bool downOk = (digitalRead(PIN_BTN_DOWN) == HIGH);
    bool enterOk = (digitalRead(PIN_BTN_ENTER) == HIGH);
    bool exitOk = (digitalRead(PIN_BTN_EXIT) == HIGH);

    LOG_FMT(LOG_LEVEL_INFO, "UP: %s", upOk ? "OK (HIGH)" : "PRESSED or ERROR");
    LOG_FMT(LOG_LEVEL_INFO, "DOWN: %s", downOk ? "OK (HIGH)" : "PRESSED or ERROR");
    LOG_FMT(LOG_LEVEL_INFO, "ENTER: %s", enterOk ? "OK (HIGH)" : "PRESSED or ERROR");
    LOG_FMT(LOG_LEVEL_INFO, "EXIT: %s", exitOk ? "OK (HIGH)" : "PRESSED or ERROR");
    LOG_FMT(LOG_LEVEL_INFO, "DOOR: %s", digitalRead(PIN_DOOR) ? "OPEN" : "CLOSED");

    bool allButtonsOk = upOk && downOk && enterOk && exitOk;

    if (!allButtonsOk) {
        log_msg(LOG_LEVEL_WARN, "WARNING: Some buttons may be stuck or have wiring issues");
    } else {
        log_msg(LOG_LEVEL_INFO, "All buttons idle (pull-up HIGH) - OK");
    }

    buzzerBeep(1, 100, 0);

    log_msg(LOG_LEVEL_INFO, "=== TEMPERATURE SENSORS TEST ===");

    sensors.requestTemperatures();
    delay(1000);

    int sensorCount = sensors.getDeviceCount();
    bool sensor1Ok = false;
    bool sensor2Ok = false;

    if (sensorCount >= 1) {
        double temp1 = sensors.getTempCByIndex(0);
        if (temp1 != DEVICE_DISCONNECTED_C && temp1 > -20 && temp1 < 100) {
            LOG_FMT(LOG_LEVEL_INFO, "Sensor 1: %.1f C - OK", temp1);
            sensor1Ok = true;
        } else {
            log_msg(LOG_LEVEL_ERROR, "Sensor 1: FAILED or invalid reading");
        }
    }

    if (sensorCount >= 2) {
        double temp2 = sensors.getTempCByIndex(1);
        if (temp2 != DEVICE_DISCONNECTED_C && temp2 > -20 && temp2 < 100) {
            LOG_FMT(LOG_LEVEL_INFO, "Sensor 2: %.1f C - OK", temp2);
            sensor2Ok = true;
        } else {
            log_msg(LOG_LEVEL_WARN, "Sensor 2: Not connected or invalid");
        }
    } else {
        log_msg(LOG_LEVEL_WARN, "Only 1 sensor detected (minimum for basic operation)");
    }

    log_msg(LOG_LEVEL_INFO, "=== OUTPUT TEST ===");

    testOutput(PIN_SSR1, "Heater 1");
    delay(50);
    testOutput(PIN_SSR2, "Heater 2");
    delay(50);
    testOutput(PIN_SSR3, "Heater 3");
    delay(50);
    testOutput(PIN_FAN, "Fan");
    delay(50);

    allOutputsOff();

    log_msg(LOG_LEVEL_INFO, "=== STARTUP SELF-TEST SUMMARY ===");
    LOG_FMT(LOG_LEVEL_INFO, "Buttons: %s", allButtonsOk ? "ALL OK" : "CHECK CONNECTIONS");
    LOG_FMT(LOG_LEVEL_INFO, "Sensors: %s (%d detected)", sensor1Ok ? "OK" : "FAILED", sensorCount);
    log_msg(LOG_LEVEL_INFO, "Outputs: TESTED");
    LOG_FMT(LOG_LEVEL_INFO, "Door: %s", digitalRead(PIN_DOOR) ? "OPEN" : "CLOSED");

    if (allButtonsOk && sensor1Ok) {
        buzzerBeep(2, 100, 50);
    } else {
        buzzerBeep(4, 150, 100);
    }

    log_msg(LOG_LEVEL_INFO, "Self-test completed");
}

void testOutput(int pin, const char* name) {
    digitalWrite(pin, HIGH);
    delay(50);
    digitalWrite(pin, LOW);
    LOG_FMT(LOG_LEVEL_INFO, "%s test: OK", name);
}

void testButton(int pin, const char* name) {
    bool state = digitalRead(pin);
    LOG_FMT(LOG_LEVEL_INFO, "%s: %s", name, state ? "HIGH" : "LOW");
}

void enterLowPowerMode() {
    log_msg(LOG_LEVEL_INFO, "Entering low power mode");

    WiFi.mode(WIFI_OFF);
    SD.end();

    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_DOOR, LOW);
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_BTN_ENTER, ESP_EXT1_WAKEUP_ANY_HIGH);

    log_msg(LOG_LEVEL_INFO, "Entering deep sleep...");
    delay(100);
    esp_deep_sleep_start();
}

bool shouldEnterLowPower() {
    static unsigned long lastActivity = millis();

    // [FIX] Sprawdzenie wyniku state_lock
    if (!state_lock()) return false;
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
