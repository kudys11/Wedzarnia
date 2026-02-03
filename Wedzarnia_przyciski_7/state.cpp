// state.cpp
#include "state.h"

// Definicje obiektów globalnych
Adafruit_ST7735 display(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);

double pidInput, pidSetpoint;
double pidOutput = 0;  // Teraz zdefiniowane globalnie
PID pid(&pidInput, &pidOutput, &pidSetpoint, CFG_Kp, CFG_Ki, CFG_Kd, DIRECT);

SemaphoreHandle_t stateMutex = NULL;
SemaphoreHandle_t outputMutex = NULL;
SemaphoreHandle_t heaterMutex = NULL;  // NOWE

// Definicje zmiennych stanu
volatile ProcessState g_currentState = ProcessState::IDLE;
RunMode g_lastRunMode = RunMode::MODE_AUTO;
volatile double g_tSet = 70.0;
volatile double g_tChamber = 25.0;
volatile double g_tMeat = 25.0;
volatile int g_powerMode = 1;
volatile int g_manualSmokePwm = 0;
volatile int g_fanMode = 1;
volatile unsigned long g_fanOnTime = CFG_FAN_ON_DEFAULT_MS;
volatile unsigned long g_fanOffTime = CFG_FAN_OFF_DEFAULT_MS;
volatile bool g_doorOpen = false;
volatile bool g_errorSensor = false;
volatile bool g_errorOverheat = false;
volatile bool g_errorProfile = false;

Step g_profile[MAX_STEPS];
int g_stepCount = 0;
int g_currentStep = 0;
unsigned long g_processStartTime = 0;
unsigned long g_stepStartTime = 0;

void state_lock() { if (stateMutex) xSemaphoreTake(stateMutex, portMAX_DELAY); }
void state_unlock() { if (stateMutex) xSemaphoreGive(stateMutex); }
void output_lock() { if (outputMutex) xSemaphoreTake(outputMutex, portMAX_DELAY); }
void output_unlock() { if (outputMutex) xSemaphoreGive(outputMutex); }
void heater_lock() { if (heaterMutex) xSemaphoreTake(heaterMutex, portMAX_DELAY); }
void heater_unlock() { if (heaterMutex) xSemaphoreGive(heaterMutex); }

void init_state() {
    stateMutex = xSemaphoreCreateMutex();
    outputMutex = xSemaphoreCreateMutex();
    heaterMutex = xSemaphoreCreateMutex();  // NOWE
    
    if (!stateMutex || !outputMutex || !heaterMutex) {
        Serial.println("FATAL: Mutex creation failed!");
        while (1) delay(1000);
    }

    pid.SetMode(AUTOMATIC);
    pid.SetOutputLimits(0, 100);
    pid.SetTunings(CFG_Kp, CFG_Ki, CFG_Kd);
    pid.SetSampleTime(1000);
}


