// sensors.cpp
#include "sensors.h"
#include "config.h"
#include "state.h"
#include "outputs.h"

static unsigned long lastTempRequest = 0;
static unsigned long lastTempReadPossible = 0;
static double lastGoodTChamber = 25.0;
static double lastGoodTMeat = 25.0;
static int sensorErrorCount = 0;

void requestTemperature() {
    unsigned long now = millis();
    if (now - lastTempRequest >= TEMP_REQUEST_INTERVAL) {
        sensors.setWaitForConversion(false);
        sensors.requestTemperatures();
        lastTempRequest = now;
        lastTempReadPossible = now + TEMP_CONVERSION_TIME;
    }
}

static bool isValidTemperature(double t) {
    return (t != DEVICE_DISCONNECTED_C && t != 85.0 && t != 127.0 && t >= -20.0 && t <= 200.0);
}

void readTemperature() {
    unsigned long now = millis();
    if (lastTempReadPossible == 0 || now < lastTempReadPossible) return;
    lastTempReadPossible = 0;

    double t1 = sensors.getTempCByIndex(0);
    double t2 = sensors.getTempCByIndex(1);

    state_lock();
    if (!isValidTemperature(t1)) {
        sensorErrorCount++;
        if (sensorErrorCount >= SENSOR_ERROR_THRESHOLD) {
            g_errorSensor = true;
            if (g_currentState == ProcessState::RUNNING_AUTO || g_currentState == ProcessState::RUNNING_MANUAL) {
                g_currentState = ProcessState::PAUSE_SENSOR;
            }
        }
        g_tChamber = lastGoodTChamber;
    } else {
        sensorErrorCount = 0;
        g_tChamber = t1;
        lastGoodTChamber = t1;
        if (g_errorSensor && g_currentState == ProcessState::PAUSE_SENSOR) {
            g_errorSensor = false;
        }
    }

    if (!isValidTemperature(t2)) {
        g_tMeat = lastGoodTMeat;
    } else {
        g_tMeat = t2;
        lastGoodTMeat = t2;
    }

    if (g_tChamber > CFG_T_MAX_SOFT) {
        g_errorOverheat = true;
        g_currentState = ProcessState::PAUSE_OVERHEAT;
    }
    state_unlock();
}

void checkDoor() {
    bool nowOpen = (digitalRead(PIN_DOOR) == HIGH);
    bool shouldTurnOff = false;
    bool shouldBeep = false;
    bool shouldResume = false;

    state_lock();
    bool wasOpen = g_doorOpen;
    if (nowOpen && !wasOpen) {
        g_doorOpen = true;
        if (g_currentState == ProcessState::RUNNING_AUTO || g_currentState == ProcessState::RUNNING_MANUAL) {
            g_currentState = ProcessState::PAUSE_DOOR;
            shouldTurnOff = true;
            shouldBeep = true;
        }
    } else if (!nowOpen && wasOpen) {
        g_doorOpen = false;
        if (g_currentState == ProcessState::PAUSE_DOOR) {
            g_currentState = ProcessState::SOFT_RESUME;
            shouldResume = true;
        }
    }
    state_unlock();

    if (shouldTurnOff) { allOutputsOff(); }
    if (shouldBeep) { buzzerBeep(2, 100, 100); }
    if (shouldResume) { initHeaterEnable(); }
}
