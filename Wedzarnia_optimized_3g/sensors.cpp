// sensors.cpp - Oczyszczona wersja (bez HA)
#include "sensors.h"
#include "config.h"
#include "state.h"
#include "outputs.h"
#include <nvs_flash.h>
#include <nvs.h>

// Struktura dla cache'owanych odczytów
struct CachedReading {
    double value;
    unsigned long timestamp;
    bool valid;
    int readAttempts;
};

static unsigned long lastTempRequest = 0;
static unsigned long lastTempReadPossible = 0;
static CachedReading cachedChamber = {25.0, 0, false, 0};
static CachedReading cachedMeat = {25.0, 0, false, 0};
static int sensorErrorCount = 0;

// STAŁE PRZYPISANIA CZUJNIKÓW
uint8_t sensorAddresses[2][8];
bool sensorsIdentified = false;
int chamberSensorIndex = DEFAULT_CHAMBER_SENSOR;
int meatSensorIndex = DEFAULT_MEAT_SENSOR;

// ======================================================
// FUNKCJE DO IDENTYFIKACJI I PRZYPISYWANIA CZUJNIKÓW
// ======================================================

void identifyAndAssignSensors() {
    if (sensorsIdentified) return;
    
    int deviceCount = sensors.getDeviceCount();
    log_msg(LOG_LEVEL_INFO, "Identifying " + String(deviceCount) + " sensor(s)...");
    
    if (deviceCount >= 2) {
        // Pobierz adresy wszystkich czujników
        for (int i = 0; i < deviceCount && i < 2; i++) {
            if (sensors.getAddress(sensorAddresses[i], i)) {
                char addrStr[17];
                sprintf(addrStr, "%02X%02X%02X%02X%02X%02X%02X%02X",
                        sensorAddresses[i][0], sensorAddresses[i][1],
                        sensorAddresses[i][2], sensorAddresses[i][3],
                        sensorAddresses[i][4], sensorAddresses[i][5],
                        sensorAddresses[i][6], sensorAddresses[i][7]);
                log_msg(LOG_LEVEL_INFO, "Sensor " + String(i) + ": " + String(addrStr));
            }
        }
        
        // Sprawdź czy mamy zapisane przypisania w NVS
        nvs_handle_t nvsHandle;
        if (nvs_open("sensor_config", NVS_READONLY, &nvsHandle) == ESP_OK) {
            uint8_t savedChamberIndex, savedMeatIndex;
            if (nvs_get_u8(nvsHandle, "chamber_idx", &savedChamberIndex) == ESP_OK &&
                nvs_get_u8(nvsHandle, "meat_idx", &savedMeatIndex) == ESP_OK) {
                
                chamberSensorIndex = savedChamberIndex;
                meatSensorIndex = savedMeatIndex;
                sensorsIdentified = true;
                log_msg(LOG_LEVEL_INFO, "Loaded sensor assignments from NVS");
                nvs_close(nvsHandle);
                return;
            }
            nvs_close(nvsHandle);
        }
        
        // BRAK ZAPISANYCH PRZYPISAŃ - PRZYPISZ DOMYŚLNE
        chamberSensorIndex = DEFAULT_CHAMBER_SENSOR;
        meatSensorIndex = DEFAULT_MEAT_SENSOR;
        
        // Zapisz do NVS
        if (nvs_open("sensor_config", NVS_READWRITE, &nvsHandle) == ESP_OK) {
            nvs_set_u8(nvsHandle, "chamber_idx", chamberSensorIndex);
            nvs_set_u8(nvsHandle, "meat_idx", meatSensorIndex);
            nvs_commit(nvsHandle);
            nvs_close(nvsHandle);
        }
        
        sensorsIdentified = true;
        log_msg(LOG_LEVEL_INFO, "Assigned: Sensor " + String(chamberSensorIndex) + " = CHAMBER");
        log_msg(LOG_LEVEL_INFO, "Assigned: Sensor " + String(meatSensorIndex) + " = MEAT");
        
        // Długi sygnał potwierdzający
        buzzerBeep(3, 200, 100);
    } else {
        log_msg(LOG_LEVEL_WARN, "Need at least 2 sensors for proper assignment");
        sensorsIdentified = false;
    }
}

// Funkcja do ręcznej zmiany przypisań
void reassignSensors(int newChamberIndex, int newMeatIndex) {
    if (newChamberIndex == newMeatIndex) {
        log_msg(LOG_LEVEL_ERROR, "Cannot assign same sensor to both chamber and meat!");
        return;
    }
    
    chamberSensorIndex = newChamberIndex;
    meatSensorIndex = newMeatIndex;
    
    // Zapisz do NVS
    nvs_handle_t nvsHandle;
    if (nvs_open("sensor_config", NVS_READWRITE, &nvsHandle) == ESP_OK) {
        nvs_set_u8(nvsHandle, "chamber_idx", chamberSensorIndex);
        nvs_set_u8(nvsHandle, "meat_idx", meatSensorIndex);
        nvs_commit(nvsHandle);
        nvs_close(nvsHandle);
    }
    
    log_msg(LOG_LEVEL_INFO, "Reassigned sensors: Chamber=" + String(chamberSensorIndex) + 
            ", Meat=" + String(meatSensorIndex));
    buzzerBeep(2, 100, 100);
}

// ======================================================
// GŁÓWNE FUNKCJE CZUJNIKÓW
// ======================================================

void requestTemperature() {
    unsigned long now = millis();
    if (now - lastTempRequest >= TEMP_REQUEST_INTERVAL) {
        sensors.setWaitForConversion(false);
        if (sensors.requestTemperatures()) {
            lastTempRequest = now;
            lastTempReadPossible = now + TEMP_CONVERSION_TIME;
        } else {
            log_msg(LOG_LEVEL_WARN, "Temperature request failed");
        }
    }
}

static bool isValidTemperature(double t) {
    return (t != DEVICE_DISCONNECTED_C && 
            t != 85.0 && 
            t != 127.0 && 
            t >= -20.0 && 
            t <= 200.0);
}

// Nowa funkcja z timeout
static double readTempWithTimeout(uint8_t sensorIndex) {
    unsigned long start = millis();
    double temp = DEVICE_DISCONNECTED_C;
    
    while (millis() - start < SENSOR_READ_TIMEOUT) {
        temp = sensors.getTempCByIndex(sensorIndex);
        if (temp != DEVICE_DISCONNECTED_C && temp != 85.0 && temp != 127.0) {
            break;
        }
        delay(1);
    }
    
    return temp;
}

void readTemperature() {
    unsigned long now = millis();
    if (lastTempReadPossible == 0 || now < lastTempReadPossible) return;
    lastTempReadPossible = 0;

    // Upewnij się że czujniki są przypisane
    if (!sensorsIdentified) {
        identifyAndAssignSensors();
        if (!sensorsIdentified) {
            log_msg(LOG_LEVEL_WARN, "Sensors not identified, using defaults");
            chamberSensorIndex = DEFAULT_CHAMBER_SENSOR;
            meatSensorIndex = DEFAULT_MEAT_SENSOR;
        }
    }

    // Odczyt z PRZYPISANYCH CZUJNIKÓW
    double tChamber = readTempWithTimeout(chamberSensorIndex);
    double tMeat = readTempWithTimeout(meatSensorIndex);

    bool t1Valid = isValidTemperature(tChamber);
    bool t2Valid = isValidTemperature(tMeat);

    // Aktualizacja cache dla czujnika komory
    if (!t1Valid) {
        sensorErrorCount++;
        cachedChamber.readAttempts++;
        
        if (sensorErrorCount >= SENSOR_ERROR_THRESHOLD) {
            if (state_lock()) {
                g_errorSensor = true;
                if (g_currentState == ProcessState::RUNNING_AUTO || 
                    g_currentState == ProcessState::RUNNING_MANUAL) {
                    g_currentState = ProcessState::PAUSE_SENSOR;
                    log_msg(LOG_LEVEL_ERROR, "Sensor error - pausing process");
                }
                state_unlock();
            }
        }
        
        // Użyj ostatniej dobrej wartości z cache
        if (cachedChamber.valid) {
            if (state_lock()) {
                g_tChamber = cachedChamber.value;
                state_unlock();
            }
            log_msg(LOG_LEVEL_WARN, "Using cached chamber temp: " + String(cachedChamber.value));
        }
    } else {
        sensorErrorCount = 0;
        cachedChamber.value = tChamber;
        cachedChamber.timestamp = now;
        cachedChamber.valid = true;
        cachedChamber.readAttempts = 0;
        
        if (state_lock()) {
            g_tChamber = tChamber;
            // Jeśli poprzednio był błąd czujnika i teraz jest OK, wyczyść błąd
            if (g_errorSensor && g_currentState == ProcessState::PAUSE_SENSOR) {
                g_errorSensor = false;
                log_msg(LOG_LEVEL_INFO, "Sensor recovered");
            }
            state_unlock();
        }
    }

    // Aktualizacja cache dla czujnika mięsa
    if (!t2Valid) {
        // Użyj ostatniej dobrej wartości z cache
        if (cachedMeat.valid) {
            if (state_lock()) {
                g_tMeat = cachedMeat.value;
                state_unlock();
            }
        }
    } else {
        cachedMeat.value = tMeat;
        cachedMeat.timestamp = now;
        cachedMeat.valid = true;
        cachedMeat.readAttempts = 0;
        
        if (state_lock()) {
            g_tMeat = tMeat;
            state_unlock();
        }
    }

    // Sprawdzenie przegrzania
    if (state_lock()) {
        if (g_tChamber > CFG_T_MAX_SOFT) {
            g_errorOverheat = true;
            g_currentState = ProcessState::PAUSE_OVERHEAT;
            String alertMsg = "OVERHEAT detected: " + String(g_tChamber, 1) + "°C";
            log_msg(LOG_LEVEL_ERROR, alertMsg);
        }
        state_unlock();
    }
}

void checkDoor() {
    bool nowOpen = (digitalRead(PIN_DOOR) == HIGH);
    bool shouldTurnOff = false;
    bool shouldBeep = false;
    bool shouldResume = false;

    if (state_lock()) {
        bool wasOpen = g_doorOpen;
        if (nowOpen && !wasOpen) {
            g_doorOpen = true;
            if (g_currentState == ProcessState::RUNNING_AUTO || 
                g_currentState == ProcessState::RUNNING_MANUAL) {
                g_currentState = ProcessState::PAUSE_DOOR;
                g_processStats.pauseCount++;
                shouldTurnOff = true;
                shouldBeep = true;
                log_msg(LOG_LEVEL_INFO, "Door opened - pausing");
            }
        } else if (!nowOpen && wasOpen) {
            g_doorOpen = false;
            if (g_currentState == ProcessState::PAUSE_DOOR) {
                g_currentState = ProcessState::SOFT_RESUME;
                shouldResume = true;
                log_msg(LOG_LEVEL_INFO, "Door closed - resuming");
            }
        }
        state_unlock();
    }

    if (shouldTurnOff) { allOutputsOff(); }
    if (shouldBeep) { buzzerBeep(2, 100, 100); }
    if (shouldResume) { initHeaterEnable(); }
}

unsigned long getSensorCacheAge() {
    unsigned long now = millis();
    return cachedChamber.valid ? (now - cachedChamber.timestamp) : 0xFFFFFFFF;
}

void forceSensorRead() {
    lastTempRequest = 0;
    lastTempReadPossible = 0;
}

// Nowa funkcja diagnostyczna
String getSensorDiagnostics() {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), 
        "Chamber: %.1f°C (sensor: %d, age: %lus, valid: %d)\n"
        "Meat: %.1f°C (sensor: %d, age: %lus, valid: %d)\n"
        "Error count: %d, Identified: %s",
        cachedChamber.value, chamberSensorIndex, getSensorCacheAge()/1000, cachedChamber.valid,
        cachedMeat.value, meatSensorIndex, cachedMeat.valid ? (millis() - cachedMeat.timestamp)/1000 : 0, 
        cachedMeat.valid,
        sensorErrorCount,
        sensorsIdentified ? "YES" : "NO");
    return String(buffer);
}

// Funkcja do pobrania informacji o przypisaniach
String getSensorAssignmentInfo() {
    String info = "Sensor Assignments:\n";
    info += "  Chamber: Sensor " + String(chamberSensorIndex) + "\n";
    info += "  Meat: Sensor " + String(meatSensorIndex) + "\n";
    info += "  Total sensors: " + String(sensors.getDeviceCount()) + "\n";
    info += "  Identified: " + String(sensorsIdentified ? "YES" : "NO");
    return info;
}

// Funkcja do automatycznego wykrywania i przypisywania
bool autoDetectAndAssignSensors() {
    int deviceCount = sensors.getDeviceCount();
    if (deviceCount < 2) {
        log_msg(LOG_LEVEL_ERROR, "Need at least 2 sensors for auto-detection");
        return false;
    }
    
    // Wymuś ponowną identyfikację
    sensorsIdentified = false;
    identifyAndAssignSensors();
    
    return sensorsIdentified;
}

// ======================================================
// FUNKCJE DOSTĘPOWE DLA WEB SERVERA
// ======================================================

int getChamberSensorIndex() {
    return chamberSensorIndex;
}

int getMeatSensorIndex() {
    return meatSensorIndex;
}

int getTotalSensorCount() {
    return sensors.getDeviceCount();
}

bool areSensorsIdentified() {
    return sensorsIdentified;
}
