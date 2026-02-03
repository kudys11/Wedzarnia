// state.h
#pragma once
#include <Adafruit_ST7735.h>
#include <DallasTemperature.h>
#include <PID_v1.h>
#include <WebServer.h>
#include "config.h"

// Deklaracje extern dla obiektów globalnych
extern Adafruit_ST7735 display;
extern WebServer server;
extern OneWire oneWire;
extern DallasTemperature sensors;
extern PID pid;
extern SemaphoreHandle_t stateMutex;
extern SemaphoreHandle_t outputMutex;
extern SemaphoreHandle_t heaterMutex;  // NOWE: mutex dla HeaterEnable

// PID output - przeniesione z extern w funkcjach
extern double pidOutput;

// Deklaracje extern dla zmiennych stanu
extern volatile ProcessState g_currentState;
extern RunMode g_lastRunMode;
extern volatile double g_tSet;
extern volatile double g_tChamber;
extern volatile double g_tMeat;
extern volatile int g_powerMode;
extern volatile int g_manualSmokePwm;
extern volatile int g_fanMode;
extern volatile unsigned long g_fanOnTime;
extern volatile unsigned long g_fanOffTime;
extern volatile bool g_doorOpen;
extern volatile bool g_errorSensor;
extern volatile bool g_errorOverheat;
extern volatile bool g_errorProfile;

extern Step g_profile[MAX_STEPS];
extern int g_stepCount;
extern int g_currentStep;
extern unsigned long g_processStartTime;
extern unsigned long g_stepStartTime;

// Funkcje pomocnicze do blokowania
void state_lock();
void state_unlock();
void output_lock();
void output_unlock();
void heater_lock();   // NOWE
void heater_unlock(); // NOWE

void init_state();

bool storage_reinit_sd();
String storage_get_profile_as_json(const char* profileName);
