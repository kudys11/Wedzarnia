// process.h - Zmodernizowana wersja
#pragma once
#include <Arduino.h>

// Główne funkcje procesu
void process_run_control_logic();
void process_start_auto();
void process_start_manual();
void process_resume();
void applyCurrentStep();

// Funkcje kontrolne
void process_force_next_step();

// Nowe funkcje dla adaptacyjnego PID
String getPidParameters();
void resetAdaptivePid();