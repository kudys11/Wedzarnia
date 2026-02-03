// process.cpp
#include "process.h"
#include "config.h"
#include "state.h"
#include "outputs.h"
#include "ui.h"

static void handleAutoMode() {
    // Walidacja zakresu PRZED dostępem do tablicy
    state_lock();
    int step = g_currentStep;
    int count = g_stepCount;
    state_unlock();
    
    if (step < 0 || step >= count) return;

    Step& s = g_profile[step];
    unsigned long elapsed = millis() - g_stepStartTime;

    state_lock();
    double meat = g_tMeat;
    state_unlock();

    bool timeOk = (elapsed >= s.minTimeMs);
    bool meatOk = (!s.useMeatTemp) || (meat >= s.tMeatTarget);

    if (timeOk && meatOk) {
        g_currentStep++;
        if (g_currentStep >= g_stepCount) {
            state_lock();
            g_currentState = ProcessState::PAUSE_USER;
            state_unlock();
            allOutputsOff();
            buzzerBeep(3, 200, 200);
            Serial.println("Profile completed!");
        } else {
            applyCurrentStep();
            buzzerBeep(2, 100, 100);
        }
    }
    
    // Sterowanie wyjściami w trybie auto - z walidacją!
    handleFanLogic();
    
    state_lock();
    step = g_currentStep;
    count = g_stepCount;
    state_unlock();
    
    if (step >= 0 && step < count) {
        output_lock();
        ledcWrite(PIN_SMOKE_FAN, g_profile[step].smokePwm);
        output_unlock();
    }
}

static void handleManualMode() {
    handleFanLogic();
    state_lock();
    int smoke = g_manualSmokePwm;
    state_unlock();
    output_lock();
    ledcWrite(PIN_SMOKE_FAN, smoke);
    output_unlock();
}

void applyCurrentStep() {
    state_lock();
    int step = g_currentStep;
    int count = g_stepCount;
    state_unlock();
    
    if (step < 0 || step >= count) return;
    
    Step& s = g_profile[step];

    state_lock();
    g_tSet = s.tSet;
    g_powerMode = s.powerMode;
    g_manualSmokePwm = s.smokePwm;
    g_fanMode = s.fanMode;
    g_fanOnTime = s.fanOnTime;
    g_fanOffTime = s.fanOffTime;
    state_unlock();

    g_stepStartTime = millis();
    Serial.printf("Step %d: %s, T=%.1f, P=%d\n", step, s.name, s.tSet, s.powerMode);
    ui_force_redraw();
}

void process_start_auto() {
    g_currentStep = 0;
    applyCurrentStep();
    initHeaterEnable();
    g_processStartTime = millis();
    state_lock();
    g_currentState = ProcessState::RUNNING_AUTO;
    g_lastRunMode = RunMode::MODE_AUTO;
    state_unlock();
}

void process_start_manual() {
    state_lock();
    g_tSet = 70;
    g_powerMode = 2;
    g_manualSmokePwm = 0;
    g_fanMode = 1;
    state_unlock();
    initHeaterEnable();
    g_processStartTime = millis();
    state_lock();
    g_currentState = ProcessState::RUNNING_MANUAL;
    g_lastRunMode = RunMode::MODE_MANUAL;
    state_unlock();
}

void process_resume() {
    initHeaterEnable();
    state_lock();
    g_currentState = ProcessState::SOFT_RESUME;
    state_unlock();
}

void process_run_control_logic() {
    extern double pidInput, pidSetpoint;
    
    state_lock();
    ProcessState st = g_currentState;
    pidInput = g_tChamber;
    pidSetpoint = g_tSet;
    state_unlock();

    // Sprawdzenie maksymalnego czasu procesu
    if ((st == ProcessState::RUNNING_AUTO || st == ProcessState::RUNNING_MANUAL) && 
        (millis() - g_processStartTime > CFG_MAX_PROCESS_TIME_MS)) {
        state_lock();
        g_currentState = ProcessState::PAUSE_USER;
        state_unlock();
        allOutputsOff();
        buzzerBeep(4, 150, 150);
        Serial.println("Max process time reached!");
        return;
    }

    switch (st) {
        case ProcessState::RUNNING_AUTO:
            pid.Compute();
            applySoftEnable();
            mapPowerToHeaters();
            handleAutoMode();
            break;

        case ProcessState::RUNNING_MANUAL:
            pid.Compute();
            applySoftEnable();
            mapPowerToHeaters();
            handleManualMode();
            break;

        case ProcessState::SOFT_RESUME:
            pid.Compute();
            applySoftEnable();
            mapPowerToHeaters();
            
            // POPRAWKA: Czekaj aż wszystkie grzałki będą gotowe!
            if (areHeatersReady()) {
                state_lock();
                g_currentState = (g_lastRunMode == RunMode::MODE_AUTO) 
                    ? ProcessState::RUNNING_AUTO 
                    : ProcessState::RUNNING_MANUAL;
                state_unlock();
                Serial.println("Resumed from pause");
            }
            break;

        case ProcessState::IDLE:
        case ProcessState::PAUSE_DOOR:
        case ProcessState::PAUSE_SENSOR:
        case ProcessState::PAUSE_OVERHEAT:
        case ProcessState::PAUSE_USER:
        case ProcessState::ERROR_PROFILE:
            allOutputsOff();
            break;
    }
}
