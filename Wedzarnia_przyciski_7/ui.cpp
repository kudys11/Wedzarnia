#include "ui.h"
#include "config.h"
#include "state.h"
#include "outputs.h"
#include "storage.h"
#include "process.h"
#include <climits>
#include <vector>
#include <ArduinoJson.h>

// ============================================================
// ZMIENNE STANU INTERFEJSU UŻYTKOWNIKA (UI)
// ============================================================
static UiState currentUiState = UiState::UI_STATE_IDLE;
static int mainMenuIndex = 0;
static const int MAIN_MENU_ITEMS = 3;
static int sourceMenuIndex = 0;
static const int SOURCE_MENU_ITEMS = 2;
static std::vector<String> profileList;
static int profileMenuIndex = 0;
static bool profilesLoading = false;
static int manualEditIndex = 0;
static const int MANUAL_EDIT_ITEMS = 5;
static bool editingFanOnTime = true;
static bool confirmSelection = false;
static bool force_redraw = true;

// Cache wartości do optymalizacji wyświetlania
static double last_tc = -99, last_tm = -99, last_ts = -99;
static String last_state_str = "";
static String last_step_name = "";
static String last_elapsed_str = "";
static String last_remaining_str = "";


// ============================================================
// INICJALIZACJA I FUNKCJE POMOCNICZE
// ============================================================
void ui_init() {}
void ui_force_redraw() { force_redraw = true; }

const char* getStateStringForDisplay(ProcessState st) {
    switch (st) {
        case ProcessState::IDLE: return "Czuwanie";
        case ProcessState::RUNNING_AUTO: return "AUTO";
        case ProcessState::RUNNING_MANUAL: return "MANUAL";
        case ProcessState::PAUSE_DOOR: return "Pauza: Drzwi";
        case ProcessState::PAUSE_SENSOR: return "Pauza: Czujnik";
        case ProcessState::PAUSE_OVERHEAT: return "Pauza: Przegrzanie";
        case ProcessState::PAUSE_USER: return "PAUZA";
        case ProcessState::ERROR_PROFILE: return "Blad Profilu";
        case ProcessState::SOFT_RESUME: return "Wznawianie...";
        default: return "Nieznany";
    }
}
void formatTime(char* buf, size_t len, unsigned long totalSeconds) {
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    snprintf(buf, len, "%02d:%02d:%02d", hours, minutes, seconds);
}

static void updateText(int16_t x, int16_t y, int16_t w, int16_t h, const String& oldText, const String& newText, uint16_t color, uint8_t textSize) {
    if (oldText != newText || force_redraw) {
        display.setTextSize(textSize);
        display.fillRect(x, y, w, h, ST77XX_BLACK);
        display.setCursor(x, y);
        display.setTextColor(color);
        display.print(newText);
    }
}

// ============================================================
// GŁÓWNA PĘTLA OBSŁUGI PRZYCISKÓW
// ============================================================
void ui_handle_buttons() {
    struct Button { const uint8_t PIN; bool lastState; unsigned long lastPressTime; };
    static Button buttons[] = { {PIN_BTN_UP, HIGH, 0}, {PIN_BTN_DOWN, HIGH, 0}, {PIN_BTN_ENTER, HIGH, 0}, {PIN_BTN_EXIT, HIGH, 0} };
    const unsigned long DEBOUNCE_TIME = 200;
    unsigned long now = millis();

    for (int i = 0; i < 4; ++i) {
        bool currentState = digitalRead(buttons[i].PIN);
        if (currentState == LOW && buttons[i].lastState == HIGH && (now - buttons[i].lastPressTime > DEBOUNCE_TIME)) {
            buttons[i].lastPressTime = now;
            buzzerBeep(1, 50, 0);
            force_redraw = true;
            int pin = buttons[i].PIN;

            state_lock();
            ProcessState proc_st = g_currentState;
            state_unlock();

            if (proc_st != ProcessState::IDLE && currentUiState != UiState::UI_STATE_IDLE && pin == PIN_BTN_EXIT) {
                currentUiState = UiState::UI_STATE_IDLE;
            } else {
                switch (currentUiState) {
                    case UiState::UI_STATE_IDLE:
                        if (pin == PIN_BTN_ENTER && proc_st == ProcessState::IDLE) { currentUiState = UiState::UI_STATE_MENU_MAIN; mainMenuIndex = 0; }
                        if (pin == PIN_BTN_EXIT && proc_st != ProcessState::IDLE) { currentUiState = UiState::UI_STATE_CONFIRM_ACTION; mainMenuIndex = 2; confirmSelection = false; }
                        break;
                    case UiState::UI_STATE_MENU_MAIN:
                        if (pin == PIN_BTN_UP) mainMenuIndex = (mainMenuIndex - 1 + MAIN_MENU_ITEMS) % MAIN_MENU_ITEMS;
                        else if (pin == PIN_BTN_DOWN) mainMenuIndex = (mainMenuIndex + 1) % MAIN_MENU_ITEMS;
                        else if (pin == PIN_BTN_EXIT) currentUiState = UiState::UI_STATE_IDLE;
                        else if (pin == PIN_BTN_ENTER) {
                            if (mainMenuIndex == 0) { currentUiState = UiState::UI_STATE_MENU_SOURCE; sourceMenuIndex = 0; } 
                            else if (mainMenuIndex == 1) { currentUiState = UiState::UI_STATE_EDIT_MANUAL; manualEditIndex = 0; } 
                            else if (mainMenuIndex == 2) { confirmSelection = false; currentUiState = UiState::UI_STATE_CONFIRM_ACTION; }
                        }
                        break;
                    case UiState::UI_STATE_MENU_SOURCE:
                        if (pin == PIN_BTN_UP || pin == PIN_BTN_DOWN) sourceMenuIndex = (sourceMenuIndex + 1) % SOURCE_MENU_ITEMS;
                        else if (pin == PIN_BTN_EXIT) currentUiState = UiState::UI_STATE_MENU_MAIN;
                        else if (pin == PIN_BTN_ENTER) { profileMenuIndex = 0; profilesLoading = true; profileList.clear(); currentUiState = UiState::UI_STATE_MENU_PROFILES; }
                        break;
                    case UiState::UI_STATE_MENU_PROFILES:
                        { 
                            if (profilesLoading) { if (pin == PIN_BTN_EXIT) { currentUiState = UiState::UI_STATE_MENU_SOURCE; } break; }
                            
                            int listSize = profileList.size();
                            if (listSize == 0) { if (pin == PIN_BTN_EXIT) { currentUiState = UiState::UI_STATE_MENU_SOURCE; } break; }
                            
                            if (pin == PIN_BTN_UP) { profileMenuIndex = (profileMenuIndex - 1 + listSize) % listSize; }
                            else if (pin == PIN_BTN_DOWN) { profileMenuIndex = (profileMenuIndex + 1) % listSize; }
                            else if (pin == PIN_BTN_EXIT) { currentUiState = UiState::UI_STATE_MENU_SOURCE; }
                            else if (pin == PIN_BTN_ENTER) {
                                String selectedProfile = profileList[profileMenuIndex];
                                String path = (sourceMenuIndex == 0) ? ("/profiles/" + selectedProfile) : ("github:" + selectedProfile);
                                storage_save_profile_path_nvs(path.c_str());
                                process_start_auto();
                                currentUiState = UiState::UI_STATE_IDLE;
                            }
                        }
                        break;
                    case UiState::UI_STATE_EDIT_MANUAL:
                        if (pin == PIN_BTN_EXIT) { currentUiState = UiState::UI_STATE_MENU_MAIN; }
                        else if (pin == PIN_BTN_ENTER) {
                            if (manualEditIndex == MANUAL_EDIT_ITEMS - 1) { process_start_manual(); currentUiState = UiState::UI_STATE_IDLE; }
                            else { manualEditIndex = (manualEditIndex + 1) % MANUAL_EDIT_ITEMS; }
                        } else if (pin == PIN_BTN_UP || pin == PIN_BTN_DOWN) {
                            int dir = (pin == PIN_BTN_UP) ? 1 : -1;
                            state_lock();
                            if (manualEditIndex == 0) g_tSet += dir;
                            else if (manualEditIndex == 1) g_powerMode += dir;
                            else if (manualEditIndex == 2) g_manualSmokePwm += dir * 5;
                            else if (manualEditIndex == 3) {
                                 if (g_fanMode == 2) { if (editingFanOnTime) g_fanOnTime += dir * 1000; else g_fanOffTime += dir * 1000; }
                                 else g_fanMode = (g_fanMode + dir + 3) % 3;
                            }
                            g_tSet = constrain(g_tSet, CFG_T_MIN_SET, CFG_T_MAX_SET);
                            g_powerMode = constrain(g_powerMode, CFG_POWERMODE_MIN, CFG_POWERMODE_MAX);
                            g_manualSmokePwm = constrain(g_manualSmokePwm, CFG_SMOKE_PWM_MIN, CFG_SMOKE_PWM_MAX);
                            if(g_fanOnTime < 1000) g_fanOnTime = 1000;
                            if(g_fanOffTime < 1000) g_fanOffTime = 1000;
                            state_unlock();
                        }
                        break;
                    case UiState::UI_STATE_CONFIRM_ACTION:
                        if (pin == PIN_BTN_UP || pin == PIN_BTN_DOWN) { confirmSelection = !confirmSelection; }
                        else if (pin == PIN_BTN_EXIT) { currentUiState = (proc_st != ProcessState::IDLE) ? UiState::UI_STATE_IDLE : UiState::UI_STATE_MENU_MAIN; }
                        else if (pin == PIN_BTN_ENTER) {
                            if (confirmSelection) { allOutputsOff(); state_lock(); g_currentState = ProcessState::IDLE; state_unlock(); }
                            currentUiState = UiState::UI_STATE_IDLE;
                        }
                        break;
                }
            }
        }
        buttons[i].lastState = currentState;
    }
}

// ============================================================
// GŁÓWNA FUNKCJA ODŚWIEŻANIA WYŚWIETLACZA
// ============================================================
void ui_update_display() {
    static unsigned long lastDisplayUpdate = 0;
    static UiState lastUiState = (UiState)-1;
    static ProcessState lastProcessState = (ProcessState)-1;

    if (millis() - lastDisplayUpdate < 200 && !force_redraw) return;
    lastDisplayUpdate = millis();
    
    state_lock();
    ProcessState st = g_currentState;
    state_unlock();

    if (st != lastProcessState) {
        currentUiState = UiState::UI_STATE_IDLE;
        force_redraw = true;
    }
    lastProcessState = st;

    if (currentUiState != lastUiState) {
        force_redraw = true;
    }

    if (force_redraw) {
        display.fillScreen(ST77XX_BLACK);
        last_tc = -99; last_tm = -99; last_ts = -99;
        last_state_str = ""; last_step_name = ""; last_elapsed_str = ""; last_remaining_str = "";
    }
    
    state_lock();
    double tc = g_tChamber;
    double tm = g_tMeat;
    double ts = g_tSet;
    int pm = g_powerMode;
    int fm = g_fanMode;
    int smoke = g_manualSmokePwm;
    unsigned long stepStartTime = g_stepStartTime;
    unsigned long processStartTime = g_processStartTime;
    int currentStep = g_currentStep;
    int stepCount = g_stepCount;
    char stepName[32];
    strncpy(stepName, (currentStep < stepCount) ? g_profile[currentStep].name : "", sizeof(stepName));
    stepName[sizeof(stepName)-1] = '\0';
    unsigned long stepTotalTimeMs = (currentStep < stepCount) ? g_profile[currentStep].minTimeMs : 0;
    state_unlock();
    
    char buf[32];
    
    if (force_redraw) {
        display.setTextWrap(false);
        display.setTextSize(1);
        display.setTextColor(ST77XX_WHITE);
        display.setCursor(0, 5);  display.print("T.kom:");
        display.setCursor(0, 27); display.print("T.mie:");
        display.drawFastHLine(0, 46, SCREEN_WIDTH, ST77XX_DARKGREY);
        display.drawFastHLine(0, 72, SCREEN_WIDTH, ST77XX_DARKGREY);
    }
    
    updateText(50, 5, 78, 16, String(last_tc, 1) + " C", String(tc, 1) + " C", ST77XX_ORANGE, 2);
    last_tc = tc;
    updateText(50, 27, 78, 16, String(last_tm, 1) + " C", String(tm, 1) + " C", ST77XX_YELLOW, 2);
    last_tm = tm;
    
    const char* stateNameStr = getStateStringForDisplay(st);
    if (st == ProcessState::RUNNING_AUTO || st == ProcessState::RUNNING_MANUAL) {
        if(force_redraw) { display.setTextSize(1); display.setCursor(0, 53); display.setTextColor(ST77XX_WHITE); display.print("T.set:"); }
        updateText(50, 53, 78, 16, String(last_ts, 1) + " C", String(ts, 1) + " C", ST77XX_CYAN, 2);
        last_ts = ts;
    } else {
        updateText(10, 53, 118, 16, last_state_str, stateNameStr, ST77XX_CYAN, 2);
    }
    last_state_str = stateNameStr;
    
    if (currentUiState != lastUiState || force_redraw) {
        display.fillRect(0, 74, SCREEN_WIDTH, 86, ST77XX_BLACK);
    }
    
    if (currentUiState == UiState::UI_STATE_IDLE) {
        if (st != ProcessState::IDLE) {
            display.setTextSize(1);
            if (st == ProcessState::RUNNING_AUTO) {
                updateText(0, 80, 128, 8, last_step_name, String("Krok: ") + stepName, ST77XX_WHITE, 1);
                last_step_name = String("Krok: ") + stepName;

                unsigned long elapsedSec = (millis() - stepStartTime) / 1000;
                formatTime(buf, sizeof(buf), elapsedSec);
                updateText(0, 95, 128, 8, last_elapsed_str, String("Uplynelo: ") + buf, ST77XX_WHITE, 1);
                last_elapsed_str = String("Uplynelo: ") + buf;

                unsigned long totalSec = stepTotalTimeMs / 1000;
                unsigned long remainingSec = (totalSec > elapsedSec) ? totalSec - elapsedSec : 0;
                formatTime(buf, sizeof(buf), remainingSec);
                updateText(0, 110, 128, 8, last_remaining_str, String("Zostalo:  ") + buf, ST77XX_WHITE, 1);
                last_remaining_str = String("Zostalo:  ") + buf;
                
                if(force_redraw) { display.setCursor(15, 140); display.print("(EXIT = Zatrzymaj)"); }

            } else if (st == ProcessState::RUNNING_MANUAL) {
                if(force_redraw) { display.setCursor(0, 90); display.print("Czas pracy:"); }
                unsigned long elapsedSec = (millis() - processStartTime) / 1000;
                formatTime(buf, sizeof(buf), elapsedSec);
                updateText(10, 105, 120, 16, last_elapsed_str, buf, ST77XX_GREEN, 2);
                last_elapsed_str = buf;
            }
        } else {
            display.setTextSize(2);
            display.setCursor(30, 90); display.print("Menu");
            display.setCursor(20, 110); display.print("(ENTER)");
        }
    } else {
        display.setTextSize(2);
        switch (currentUiState) {
            case UiState::UI_STATE_MENU_MAIN:
                display.setCursor(0, 80); display.setTextColor(mainMenuIndex == 0 ? ST77XX_GREEN : ST77XX_WHITE); display.print(">Start AUTO");
                display.setCursor(0, 105); display.setTextColor(mainMenuIndex == 1 ? ST77XX_GREEN : ST77XX_WHITE); display.print(">Start MANUAL");
                display.setCursor(0, 130); display.setTextColor(mainMenuIndex == 2 ? ST77XX_GREEN : ST77XX_WHITE); display.print(">Zatrzymaj");
                break;
            case UiState::UI_STATE_MENU_SOURCE:
                display.setCursor(10, 90); display.setTextColor(sourceMenuIndex == 0 ? ST77XX_GREEN : ST77XX_WHITE); display.print("> Karta SD");
                display.setCursor(10, 115); display.setTextColor(sourceMenuIndex == 1 ? ST77XX_GREEN : ST77XX_WHITE); display.print("> GitHub");
                break;
            case UiState::UI_STATE_MENU_PROFILES:
                if (profilesLoading) {
                    display.setCursor(10, 95); display.print("Wczytywanie...");
                    String json_str = (sourceMenuIndex == 0) ? storage_list_profiles_json() : storage_list_github_profiles_json();
                    profileList.clear();
                    DynamicJsonDocument doc(2048);
                    if (deserializeJson(doc, json_str) == DeserializationError::Ok) {
                        for (JsonVariant value : doc.as<JsonArray>()) { profileList.push_back(String(value.as<const char*>())); }
                    }
                    profilesLoading = false;
                    force_redraw = true;
                    ui_update_display(); 
                    return;
                } 
                if (profileList.empty()) {
                    display.setCursor(10, 95); display.print("Brak profili!");
                } else {
                    display.setTextSize(1);
                    for (size_t i = 0; i < profileList.size(); i++) {
                        if (i < 6) {
                             display.setCursor(0, 80 + i * 13);
                             if ((int)i == profileMenuIndex) { display.setTextColor(ST77XX_GREEN); display.print("> "); }
                             else { display.setTextColor(ST77XX_WHITE); display.print("  "); }
                             display.print(profileList[i]);
                        }
                    }
                }
                break;
            case UiState::UI_STATE_EDIT_MANUAL:
                display.setTextSize(1);
                display.setCursor(0, 80); display.setTextColor(manualEditIndex == 0 ? ST77XX_YELLOW : ST77XX_WHITE); display.printf("> Temp: %.1f C", ts);
                display.setCursor(0, 92); display.setTextColor(manualEditIndex == 1 ? ST77XX_YELLOW : ST77XX_WHITE); display.printf("> Moc: %d", pm);
                display.setCursor(0, 104); display.setTextColor(manualEditIndex == 2 ? ST77XX_YELLOW : ST77XX_WHITE); display.printf("> Dym: %d", smoke);
                display.setCursor(0, 116); display.setTextColor(manualEditIndex == 3 ? ST77XX_YELLOW : ST77XX_WHITE);
                if(fm == 0) display.print("> Went: OFF"); else if (fm == 1) display.print("> Went: ON"); else display.print("> Went: CYKL");
                display.setTextSize(2);
                display.setCursor(15, 135); display.setTextColor(manualEditIndex == 4 ? ST77XX_YELLOW : ST77XX_WHITE); display.print("[START]");
                break;
            case UiState::UI_STATE_CONFIRM_ACTION:
                display.setCursor(15, 95); display.print("Na pewno?");
                display.setTextSize(1);
                display.setCursor(10, 120); display.setTextColor(!confirmSelection ? ST77XX_GREEN : ST77XX_WHITE); display.print("[ NIE ]");
                display.setCursor(70, 120); display.setTextColor(confirmSelection ? ST77XX_GREEN : ST77XX_WHITE); display.print("[ TAK ]");
                break;
        }
    }
    
    lastUiState = currentUiState;
    force_redraw = false;
}
