// config.h
#pragma once
#include <Arduino.h>

// ======================================================
// 1. DEFINICJE PINÓW
// ======================================================
#define PIN_ONEWIRE 4
#define PIN_SSR1 12
#define PIN_SSR2 13
#define PIN_SSR3 14
#define PIN_FAN 27
#define PIN_SMOKE_FAN 26
#define PIN_DOOR 25
#define PIN_BUZZER 33
#define PIN_BTN_UP      32 
#define PIN_BTN_DOWN    17
#define PIN_BTN_ENTER   16
#define PIN_BTN_EXIT    21 // Przykładowy wolny pin
#define PIN_SD_CS 5
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 22

// ======================================================
// 2. KONFIGURACJA GLOBALNA
// ======================================================

// --- Wyświetlacz ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160
#define ST77XX_DARKGREY 0x7BEF

// --- LEDC (PWM) ---
#define LEDC_FREQ 5000
#define LEDC_RESOLUTION 8
// Kanały LEDC dla SSR i dymu (każde wyjście PWM potrzebuje osobnego kanału)
#define LEDC_CHANNEL_SSR1 0
#define LEDC_CHANNEL_SSR2 1
#define LEDC_CHANNEL_SSR3 2
#define LEDC_CHANNEL_SMOKE 3

// --- PID ---
const double CFG_Kp = 5.0;
const double CFG_Ki = 0.3;
const double CFG_Kd = 20.0;

// --- Limity ---
const double CFG_T_MAX_SOFT = 130.0;
const double CFG_T_MIN_SET = 20.0;
const double CFG_T_MAX_SET = 130.0;
const unsigned long CFG_MAX_PROCESS_TIME_MS = 12UL * 60UL * 60UL * 1000UL; // 12h
const int CFG_SMOKE_PWM_MIN = 0;
const int CFG_SMOKE_PWM_MAX = 255;
const int CFG_POWERMODE_MIN = 1;
const int CFG_POWERMODE_MAX = 3;

// --- Domyślne wartości ---
const unsigned long CFG_FAN_ON_DEFAULT_MS = 10000;
const unsigned long CFG_FAN_OFF_DEFAULT_MS = 60000;

// --- WiFi / Web ---
const char* const CFG_AP_SSID = "Wedzarnia";
const char* const CFG_AP_PASS = "12345678";

// --- GitHub Repo for Profiles ---
// Zmień na swój adres API do katalogu /profiles!
const char* const CFG_GITHUB_API_URL = "https://api.github.com/repos/kudys11/wedzarnia-przepisy/contents/profiles";

// Bazowy URL do SUROWYCH plików pozostaje taki sam
const char* const CFG_GITHUB_PROFILES_BASE_URL = "https://raw.githubusercontent.com/kudys11/wedzarnia-przepisy/main/profiles/";

// --- Watchdog ---
#define WDT_TIMEOUT 8 // sekundy
#define SOFT_WDT_TIMEOUT 30000 // milisekundy

// --- Czujniki ---
const unsigned long TEMP_REQUEST_INTERVAL = 1200;
const unsigned long TEMP_CONVERSION_TIME = 850;
const int SENSOR_ERROR_THRESHOLD = 3;

// --- Profil ---
#define MAX_STEPS 8
// Max liczba profili w liście i max długość nazwy profilu
// Nazwy dłuższe niż MAX_PROFILE_NAME_LEN-1 będą obcinane
#define MAX_PROFILES 32
#define MAX_PROFILE_NAME_LEN 64

// ======================================================
// 3. DEFINICJE TYPÓW I STRUKTUR
// ======================================================

enum class ProcessState {
    IDLE,
    RUNNING_AUTO,
    RUNNING_MANUAL,
    PAUSE_DOOR,
    PAUSE_SENSOR,
    PAUSE_OVERHEAT,
    PAUSE_USER,
    ERROR_PROFILE,
    SOFT_RESUME
};

enum class RunMode {
    MODE_AUTO,
    MODE_MANUAL
};

struct HeaterEnable {
    bool h1, h2, h3;
    unsigned long t1, t2, t3;
};

struct Step {
    char name[32];
    double tSet;
    double tMeatTarget;
    unsigned long minTimeMs;
    int powerMode;
    int smokePwm;
    int fanMode;
    unsigned long fanOnTime;
    unsigned long fanOffTime;
    bool useMeatTemp;
};
