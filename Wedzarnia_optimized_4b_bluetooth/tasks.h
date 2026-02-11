// tasks.h - Zmodernizowana wersja
#ifndef TASKS_H
#define TASKS_H

#include "Arduino.h"

// Deklaracje funkcji tworzących zadania
void tasks_create_all();

// Deklaracje samych zadań
void taskControl(void* pv);
void taskSensors(void* pv);
void taskUI(void* pv);
void taskWeb(void* pv);
void taskWiFi(void* pv);
void taskMonitor(void* pv);

// >> TUTAJ JEST TA LINIA <<
// Udostępnia funkcję inicjalizacji watchdoga, aby mogła być wywołana w setup()
void watchdog_init();

#endif // TASKS_H
