// process.cpp - Zmodernizowana wersja z adaptacyjnym PID
#include "process.h"
#include "config.h"
#include "state.h"
#include "outputs.h"
#include "ui.h"

// Struktura dla adaptacyjnego PID
struct AdaptivePID {
    double errorHistory[10] = {0};
    int historyIndex = 0;
    unsigned long lastAdaptation = 0;
    double currentKp = CFG_Kp;
    double currentKi = CFG_Ki;
    double currentKd = CFG_Kd;
};

static AdaptivePID adaptivePid;

// Historia temperatury dla predykcyjnego sterowania wentylatorem
static double tempHistory[5] = {0};
static int tempHistoryIndex = 0;

static void updateProcessStats() {
    if (!state_lock()) return;
    
    unsigned long now = millis();
    unsigned long elapsed = now - g_processStats.lastUpdate;
    
    if (g_currentState == ProcessState::RUNNING_AUTO || 
        g_currentState == ProcessState::RUNNING_MANUAL) {
        g_processStats.totalRunTime += elapsed;
        
        // Sprawdź czy grzałki są aktywne
        if (pidOutput > 5.0) {
            g_processStats.activeHeatingTime += elapsed;
        }
        
        // Oblicz średnią temperaturę (moving average)
        if (g_processStats.avgTemp == 0.0) {
            g_processStats.avgTemp = g_tChamber;
        } else {
            // Exponential moving average
            constexpr double alpha = 0.1;
            g_processStats.avgTemp = alpha * g_tChamber + (1.0 - alpha) * g_processStats.avgTemp;
        }
        
        // Oblicz pozostały czas procesu (tylko dla AUTO)
        if (g_currentState == ProcessState::RUNNING_AUTO) {
            unsigned long elapsedTotal = (now - g_processStartTime) / 1000;
            
            // Czas już wykonanych kroków
            unsigned long completedTime = 0;
            for (int i = 0; i < g_currentStep; i++) {
                if (i < g_stepCount) {
                    completedTime += g_profile[i].minTimeMs / 1000;
                }
            }
            
            // Czas pozostały w bieżącym kroku
            unsigned long stepElapsed = (now - g_stepStartTime) / 1000;
            unsigned long stepTotal = 0;
            if (g_currentStep >= 0 && g_currentStep < g_stepCount) {
                stepTotal = g_profile[g_currentStep].minTimeMs / 1000;
            }
            unsigned long stepRemaining = (stepTotal > stepElapsed) ? (stepTotal - stepElapsed) : 0;
            
            // Czas pozostałych kroków
            unsigned long futureTime = 0;
            for (int i = g_currentStep + 1; i < g_stepCount; i++) {
                futureTime += g_profile[i].minTimeMs / 1000;
            }
            
            g_processStats.remainingProcessTimeSec = stepRemaining + futureTime;
        } else {
            g_processStats.remainingProcessTimeSec = 0;
        }
    }
    
    g_processStats.lastUpdate = now;
    state_unlock();
}

// Adaptacyjne dostosowanie parametrów PID
static void adaptPidParameters() {
    unsigned long now = millis();
    if (now - adaptivePid.lastAdaptation < PID_ADAPTATION_INTERVAL) return;
    
    double currentError = pidSetpoint - pidInput;
    
    // Zapisz błąd do historii
    adaptivePid.errorHistory[adaptivePid.historyIndex] = currentError;
    adaptivePid.historyIndex = (adaptivePid.historyIndex + 1) % 10;
    
    // Oblicz zmienność błędu
    double errorMean = 0;
    double errorVariance = 0;
    int validCount = 0;
    
    for (int i = 0; i < 10; i++) {
        if (fabs(adaptivePid.errorHistory[i]) < 50) { // Ignoruj wartości odstające
            errorMean += adaptivePid.errorHistory[i];
            validCount++;
        }
    }
    
    if (validCount > 0) {
        errorMean /= validCount;
        
        for (int i = 0; i < 10; i++) {
            if (fabs(adaptivePid.errorHistory[i]) < 50) {
                errorVariance += pow(adaptivePid.errorHistory[i] - errorMean, 2);
            }
        }
        errorVariance /= validCount;
        
        // Dostosuj parametry PID w oparciu o zmienność błędu
        if (errorVariance > 5.0) { // Duże oscylacje
            adaptivePid.currentKp = CFG_Kp * 0.8;
            adaptivePid.currentKi = CFG_Ki * 0.5;
            adaptivePid.currentKd = CFG_Kd * 1.2;
        } else if (errorVariance < 0.5 && fabs(currentError) < 2.0) { // Stabilna temperatura
            adaptivePid.currentKp = CFG_Kp * 1.2;
            adaptivePid.currentKi = CFG_Ki * 0.8;
            adaptivePid.currentKd = CFG_Kd * 0.8;
        } else { // Powrót do domyślnych
            adaptivePid.currentKp = CFG_Kp;
            adaptivePid.currentKi = CFG_Ki;
            adaptivePid.currentKd = CFG_Kd;
        }
        
        // Zastosuj nowe parametry
        pid.SetTunings(adaptivePid.currentKp, adaptivePid.currentKi, adaptivePid.currentKd);
        
        if (adaptivePid.lastAdaptation > 0) { // Nie loguj przy pierwszym uruchomieniu
            log_msg(LOG_LEVEL_DEBUG, "PID adapted: Kp=" + String(adaptivePid.currentKp, 2) + 
                   ", Ki=" + String(adaptivePid.currentKi, 2) + 
                   ", Kd=" + String(adaptivePid.currentKd, 2) +
                   ", var=" + String(errorVariance, 2));
        }
    }
    
    adaptivePid.lastAdaptation = now;
}

// Predykcyjne sterowanie wentylatorem - ZMIEŃ Z "static" NA "void"
void predictiveFanControl() {
    // Zbierz historię temperatury
    double currentTemp = 0;
    if (state_lock()) {
        currentTemp = g_tChamber;
        state_unlock();
    }
    
    tempHistory[tempHistoryIndex] = currentTemp;
    tempHistoryIndex = (tempHistoryIndex + 1) % 5;
    
    // Oblicz trend temperatury
    double trend = 0;
    int validSamples = 0;
    for (int i = 1; i < 5; i++) {
        int idx = (tempHistoryIndex - i + 5) % 5;
        int prevIdx = (idx - 1 + 5) % 5;
        
        if (tempHistory[idx] > 0 && tempHistory[prevIdx] > 0) {
            trend += (tempHistory[idx] - tempHistory[prevIdx]);
            validSamples++;
        }
    }
    
    if (validSamples > 0) {
        trend /= validSamples;
        
        // Dostosuj czas cyklu wentylatora w oparciu o trend
        if (state_lock()) {
            if (g_fanMode == 2) { // Tylko w trybie cyklicznym
                if (trend > 0.5) { // Temperatura szybko rośnie
                    g_fanOnTime = min((unsigned long)(g_fanOnTime * 1.5), 30000UL); // Max 30s ON
                    g_fanOffTime = max((unsigned long)(g_fanOffTime * 0.7), 10000UL); // Min 10s OFF
                    log_msg(LOG_LEVEL_DEBUG, "Fan: temp rising fast, increasing ON time");
                } else if (trend < -0.2) { // Temperatura spada
                    g_fanOnTime = max((unsigned long)(g_fanOnTime * 0.7), 5000UL); // Min 5s ON
                    g_fanOffTime = min((unsigned long)(g_fanOffTime * 1.3), 120000UL); // Max 120s OFF
                    log_msg(LOG_LEVEL_DEBUG, "Fan: temp falling, decreasing ON time");
                } else if (fabs(trend) < 0.1 && fabs(g_tChamber - g_tSet) < 3.0) {
                    // Stabilna temperatura w pobliżu setpointu
                    g_fanOnTime = 10000UL; // Powrót do domyślnych
                    g_fanOffTime = 60000UL;
                }
            }
            state_unlock();
        }
    }
}

static void handleAutoMode() {
    if (!state_lock()) return;
    int step = g_currentStep;
    int count = g_stepCount;
    state_unlock();
    
    if (step < 0 || step >= count) {
        log_msg(LOG_LEVEL_ERROR, "Invalid step in AUTO mode: " + String(step));
        return;
    }

    Step& s = g_profile[step];
    unsigned long elapsed = millis() - g_stepStartTime;

    if (!state_lock()) return;
    double meat = g_tMeat;
    state_unlock();

    bool timeOk = (elapsed >= s.minTimeMs);
    bool meatOk = (!s.useMeatTemp) || (meat >= s.tMeatTarget);

    if (timeOk && meatOk) {
        g_currentStep++;
        if (!state_lock()) return;
        g_processStats.stepChanges++;
        state_unlock();
        
        if (g_currentStep >= g_stepCount) {
            if (state_lock()) {
                g_currentState = ProcessState::PAUSE_USER;
                state_unlock();
            }
            allOutputsOff();
            buzzerBeep(3, 200, 200);
            log_msg(LOG_LEVEL_INFO, "Profile completed!");
        } else {
            applyCurrentStep();
            buzzerBeep(2, 100, 100);
            log_msg(LOG_LEVEL_INFO, "Advanced to step " + String(g_currentStep));
        }
    }
    
    // Predykcyjne sterowanie wentylatorem
    predictiveFanControl();
    
    // Sterowanie wyjściami
    handleFanLogic();
    
    if (!state_lock()) return;
    step = g_currentStep;
    count = g_stepCount;
    state_unlock();
    
    if (step >= 0 && step < count) {
        if (output_lock()) {
            ledcWrite(PIN_SMOKE_FAN, g_profile[step].smokePwm);
            output_unlock();
        }
    }
}

static void handleManualMode() {
    // Predykcyjne sterowanie wentylatorem
    predictiveFanControl();
    
    handleFanLogic();
    if (!state_lock()) return;
    int smoke = g_manualSmokePwm;
    state_unlock();
    
    if (output_lock()) {
        ledcWrite(PIN_SMOKE_FAN, smoke);
        output_unlock();
    }
}

void applyCurrentStep() {
    if (!state_lock()) return;
    int step = g_currentStep;
    int count = g_stepCount;
    state_unlock();
    
    if (step < 0 || step >= count) {
        log_msg(LOG_LEVEL_ERROR, "Cannot apply step - invalid index: " + String(step));
        return;
    }
    
    Step& s = g_profile[step];

    if (state_lock()) {
        g_tSet = s.tSet;
        g_powerMode = s.powerMode;
        g_manualSmokePwm = s.smokePwm;
        g_fanMode = s.fanMode;
        g_fanOnTime = s.fanOnTime;
        g_fanOffTime = s.fanOffTime;
        state_unlock();
    }

    g_stepStartTime = millis();
    log_msg(LOG_LEVEL_INFO, "Step " + String(step) + ": " + String(s.name) + 
            ", T=" + String(s.tSet, 1) + ", P=" + String(s.powerMode));
    ui_force_redraw();
}

void process_start_auto() {
    g_currentStep = 0;
    applyCurrentStep();
    initHeaterEnable();
    g_processStartTime = millis();
    
    if (state_lock()) {
        g_currentState = ProcessState::RUNNING_AUTO;
        g_lastRunMode = RunMode::MODE_AUTO;
        // Reset statystyk
        g_processStats.totalRunTime = 0;
        g_processStats.activeHeatingTime = 0;
        g_processStats.stepChanges = 0;
        g_processStats.pauseCount = 0;
        g_processStats.avgTemp = 0.0;
        g_processStats.lastUpdate = millis();
        // Reset adaptacyjnego PID
        adaptivePid.currentKp = CFG_Kp;
        adaptivePid.currentKi = CFG_Ki;
        adaptivePid.currentKd = CFG_Kd;
        pid.SetTunings(CFG_Kp, CFG_Ki, CFG_Kd);
        state_unlock();
    }
    
    log_msg(LOG_LEVEL_INFO, "AUTO mode started");
	
    #ifdef ENABLE_HOME_ASSISTANT
    ha_send_alert("Start AUTO", "Rozpoczęto proces automatyczny");
    ha_send_state("running_auto");
    #endif
}

void process_start_manual() {
    if (state_lock()) {
        g_tSet = 70;
        g_powerMode = 2;
        g_manualSmokePwm = 0;
        g_fanMode = 1;
        state_unlock();
    }
    initHeaterEnable();
    g_processStartTime = millis();
    
    if (state_lock()) {
        g_currentState = ProcessState::RUNNING_MANUAL;
        g_lastRunMode = RunMode::MODE_MANUAL;
        // Reset statystyk
        g_processStats.totalRunTime = 0;
        g_processStats.activeHeatingTime = 0;
        g_processStats.stepChanges = 0;
        g_processStats.pauseCount = 0;
        g_processStats.avgTemp = 0.0;
        g_processStats.lastUpdate = millis();
        state_unlock();
    }
    
    log_msg(LOG_LEVEL_INFO, "MANUAL mode started");
    
    #ifdef ENABLE_HOME_ASSISTANT
    ha_send_alert("Start MANUAL", "Rozpoczęto proces manualny");
    ha_send_state("running_manual");
    #endif
}

void process_resume() {
    initHeaterEnable();
    if (state_lock()) {
        g_currentState = ProcessState::SOFT_RESUME;
        state_unlock();
    }
    log_msg(LOG_LEVEL_INFO, "Process resuming...");
}

void process_run_control_logic() {
    extern double pidInput, pidSetpoint;
    
    if (!state_lock()) return;
    ProcessState st = g_currentState;
    pidInput = g_tChamber;
    pidSetpoint = g_tSet;
    state_unlock();

    // Sprawdzenie maksymalnego czasu procesu
    if ((st == ProcessState::RUNNING_AUTO || st == ProcessState::RUNNING_MANUAL) && 
        (millis() - g_processStartTime > CFG_MAX_PROCESS_TIME_MS)) {
        if (state_lock()) {
            g_currentState = ProcessState::PAUSE_USER;
            state_unlock();
        }
        allOutputsOff();
        buzzerBeep(4, 150, 150);
        log_msg(LOG_LEVEL_WARN, "Max process time reached!");
        return;
    }

    switch (st) {
        case ProcessState::RUNNING_AUTO:
            adaptPidParameters(); // Adaptacyjne dostosowanie PID
            pid.Compute();
            applySoftEnable();
            mapPowerToHeaters();
            handleAutoMode();
            updateProcessStats();
            break;

        case ProcessState::RUNNING_MANUAL:
            pid.Compute();
            applySoftEnable();
            mapPowerToHeaters();
            handleManualMode();
            updateProcessStats();
            break;

        case ProcessState::SOFT_RESUME:
            pid.Compute();
            applySoftEnable();
            mapPowerToHeaters();
            
            if (areHeatersReady()) {
                if (state_lock()) {
                    g_currentState = (g_lastRunMode == RunMode::MODE_AUTO) 
                        ? ProcessState::RUNNING_AUTO 
                        : ProcessState::RUNNING_MANUAL;
                    state_unlock();
                }
                log_msg(LOG_LEVEL_INFO, "Process resumed from pause");
            }
            break;

        case ProcessState::IDLE:
        case ProcessState::PAUSE_DOOR:
        case ProcessState::PAUSE_SENSOR:
        case ProcessState::PAUSE_OVERHEAT:
        case ProcessState::PAUSE_USER:
		    allOutputsOff();
            #ifdef ENABLE_HOME_ASSISTANT
            ha_send_alert("Proces zakończony", "Proces został zakończony");
            ha_send_state("idle");
            #endif
    break;
        case ProcessState::ERROR_PROFILE:
            allOutputsOff();
            break;
    }
}

void process_force_next_step() {
    if (!state_lock()) return;
    
    if (g_currentState != ProcessState::RUNNING_AUTO) {
        log_msg(LOG_LEVEL_WARN, "Cannot skip step - not in AUTO mode");
        state_unlock();
        return;
    }
    
    int nextStep = g_currentStep + 1;
    if (nextStep >= g_stepCount) {
        log_msg(LOG_LEVEL_WARN, "Cannot skip step - already at last step");
        state_unlock();
        return;
    }
    
    state_unlock();
    
    g_currentStep = nextStep;
    applyCurrentStep();
    log_msg(LOG_LEVEL_INFO, "Step skipped to " + String(nextStep));
    buzzerBeep(1, 100, 0);
}

// Nowa funkcja: pobierz aktualne parametry PID
String getPidParameters() {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), 
             "Kp=%.2f, Ki=%.2f, Kd=%.2f (base: %.1f,%.1f,%.1f)", 
             adaptivePid.currentKp, adaptivePid.currentKi, adaptivePid.currentKd,
             CFG_Kp, CFG_Ki, CFG_Kd);
    return String(buffer);
}

// Nowa funkcja: resetuj adaptacyjny PID
void resetAdaptivePid() {
    adaptivePid.currentKp = CFG_Kp;
    adaptivePid.currentKi = CFG_Ki;
    adaptivePid.currentKd = CFG_Kd;
    pid.SetTunings(CFG_Kp, CFG_Ki, CFG_Kd);
    
    for (int i = 0; i < 10; i++) {
        adaptivePid.errorHistory[i] = 0;
    }
    adaptivePid.historyIndex = 0;
    adaptivePid.lastAdaptation = 0;
    
    log_msg(LOG_LEVEL_INFO, "Adaptive PID reset to defaults");
}