// bluetooth.cpp - Implementacja Bluetooth Classic
#include "bluetooth.h"
#include "config.h"
#include "state.h"
#include "process.h"
#include "storage.h"
#include <SD.h>

BluetoothSerial SerialBT;

// Bufor dla przychodzących komend
static String btCommandBuffer = "";
static unsigned long lastStatusSend = 0;
static const unsigned long STATUS_SEND_INTERVAL = 2000; // Co 2 sekundy

void bluetooth_init() {
    if (!SerialBT.begin("Wedzarnia_ESP32")) {
        log_msg(LOG_LEVEL_ERROR, "Bluetooth init failed!");
        return;
    }
    
    log_msg(LOG_LEVEL_INFO, "Bluetooth started: 'Wedzarnia_ESP32'");
    log_msg(LOG_LEVEL_INFO, "Waiting for connection...");
}

bool bluetooth_is_connected() {
    return SerialBT.hasClient();
}

void bluetooth_log(const String& message) {
    if (bluetooth_is_connected()) {
        SerialBT.println("LOG:" + message);
    }
}

void bluetooth_send_status() {
    if (!bluetooth_is_connected()) return;
    
    state_lock();
    
    // Format: STATUS:state,tSet,tChamber,tMeat,power,smoke,fan,step,stepCount
    String status = "STATUS:";
    
    // Stan procesu
    switch (g_currentState) {
        case ProcessState::IDLE: status += "IDLE"; break;
        case ProcessState::RUNNING_AUTO: status += "AUTO"; break;
        case ProcessState::RUNNING_MANUAL: status += "MANUAL"; break;
        case ProcessState::PAUSE_DOOR: status += "PAUSE_DOOR"; break;
        case ProcessState::PAUSE_SENSOR: status += "PAUSE_SENSOR"; break;
        case ProcessState::PAUSE_OVERHEAT: status += "PAUSE_OVERHEAT"; break;
        case ProcessState::PAUSE_USER: status += "PAUSE_USER"; break;
        case ProcessState::ERROR_PROFILE: status += "ERROR"; break;
        case ProcessState::SOFT_RESUME: status += "RESUMING"; break;
    }
    
    status += ",";
    status += String(g_tSet, 1);
    status += ",";
    status += String(g_tChamber, 1);
    status += ",";
    status += String(g_tMeat, 1);
    status += ",";
    status += String(g_powerMode);
    status += ",";
    status += String(g_manualSmokePwm);
    status += ",";
    status += String(g_fanMode);
    status += ",";
    status += String(g_currentStep);
    status += ",";
    status += String(g_stepCount);
    status += ",";
    status += String(g_doorOpen ? "1" : "0");
    status += ",";
    status += String(g_errorSensor ? "1" : "0");
    status += ",";
    status += String(g_errorOverheat ? "1" : "0");
    
    state_unlock();
    
    SerialBT.println(status);
}

void bluetooth_send_temperatures() {
    if (!bluetooth_is_connected()) return;
    
    state_lock();
    String temps = "TEMPS:";
    temps += String(g_tChamber, 2);
    temps += ",";
    temps += String(g_tMeat, 2);
    temps += ",";
    temps += String(g_tSet, 1);
    state_unlock();
    
    SerialBT.println(temps);
}

void bluetooth_send_profile_list() {
    if (!bluetooth_is_connected()) return;
    
    SerialBT.println("PROFILES_START");
    
    File root = SD.open("/profiles");
    if (!root || !root.isDirectory()) {
        SerialBT.println("PROFILES_END");
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fileName = file.name();
            if (fileName.endsWith(".prof")) {
                SerialBT.println("PROFILE:" + fileName);
            }
        }
        file = root.openNextFile();
    }
    root.close();
    
    SerialBT.println("PROFILES_END");
}

void bluetooth_parse_command(String command) {
    command.trim();
    
    if (command.length() == 0) return;
    
    log_msg(LOG_LEVEL_INFO, "BT Command: " + command);
    
    // PING-PONG test
    if (command == BTCommand::PING) {
        SerialBT.println(BTResponse::PONG);
        return;
    }
    
    // GET STATUS
    if (command == BTCommand::GET_STATUS) {
        bluetooth_send_status();
        return;
    }
    
    // GET TEMPERATURES
    if (command == BTCommand::GET_TEMPS) {
        bluetooth_send_temperatures();
        return;
    }
    
    // GET PROFILES
    if (command == BTCommand::GET_PROFILES) {
        bluetooth_send_profile_list();
        return;
    }
    
    // START AUTO
    if (command == BTCommand::START_AUTO) {
        if (state_lock()) {
            if (g_errorProfile) {
                SerialBT.println("ERROR:No profile loaded");
                state_unlock();
                return;
            }
            state_unlock();
        }
        process_start_auto();
        SerialBT.println(BTResponse::OK);
        bluetooth_log("AUTO mode started");
        return;
    }
    
    // START MANUAL
    if (command == BTCommand::START_MANUAL) {
        process_start_manual();
        SerialBT.println(BTResponse::OK);
        bluetooth_log("MANUAL mode started");
        return;
    }
    
    // STOP
    if (command == BTCommand::STOP) {
        if (state_lock()) {
            g_currentState = ProcessState::PAUSE_USER;
            state_unlock();
        }
        SerialBT.println(BTResponse::OK);
        bluetooth_log("Process stopped");
        return;
    }
    
    // PAUSE
    if (command == BTCommand::PAUSE) {
        if (state_lock()) {
            if (g_currentState == ProcessState::RUNNING_AUTO || 
                g_currentState == ProcessState::RUNNING_MANUAL) {
                g_currentState = ProcessState::PAUSE_USER;
                SerialBT.println(BTResponse::OK);
                bluetooth_log("Process paused");
            } else {
                SerialBT.println("ERROR:Not running");
            }
            state_unlock();
        }
        return;
    }
    
    // RESUME
    if (command == BTCommand::RESUME) {
        if (state_lock()) {
            if (g_currentState == ProcessState::PAUSE_USER) {
                state_unlock();
                process_resume();
                SerialBT.println(BTResponse::OK);
                bluetooth_log("Process resumed");
            } else {
                SerialBT.println("ERROR:Not paused");
                state_unlock();
            }
        }
        return;
    }
    
    // NEXT STEP
    if (command == BTCommand::NEXT_STEP) {
        process_force_next_step();
        SerialBT.println(BTResponse::OK);
        return;
    }
    
    // Komendy z parametrem (format: COMMAND:value)
    int colonIndex = command.indexOf(':');
    if (colonIndex > 0) {
        String cmd = command.substring(0, colonIndex);
        String value = command.substring(colonIndex + 1);
        
        // SET_TEMP:75.5
        if (cmd == BTCommand::SET_TEMP) {
            double temp = value.toDouble();
            if (temp >= CFG_T_MIN_SET && temp <= CFG_T_MAX_SET) {
                if (state_lock()) {
                    g_tSet = temp;
                    state_unlock();
                }
                SerialBT.println(BTResponse::OK);
                bluetooth_log("Temp set to " + String(temp, 1));
            } else {
                SerialBT.println("ERROR:Invalid temp range");
            }
            return;
        }
        
        // SET_POWER:2
        if (cmd == BTCommand::SET_POWER) {
            int power = value.toInt();
            if (power >= CFG_POWERMODE_MIN && power <= CFG_POWERMODE_MAX) {
                if (state_lock()) {
                    g_powerMode = power;
                    state_unlock();
                }
                SerialBT.println(BTResponse::OK);
                bluetooth_log("Power mode set to " + String(power));
            } else {
                SerialBT.println("ERROR:Invalid power mode");
            }
            return;
        }
        
        // SET_SMOKE:128
        if (cmd == BTCommand::SET_SMOKE) {
            int smoke = value.toInt();
            if (smoke >= CFG_SMOKE_PWM_MIN && smoke <= CFG_SMOKE_PWM_MAX) {
                if (state_lock()) {
                    g_manualSmokePwm = smoke;
                    state_unlock();
                }
                SerialBT.println(BTResponse::OK);
                bluetooth_log("Smoke PWM set to " + String(smoke));
            } else {
                SerialBT.println("ERROR:Invalid smoke PWM");
            }
            return;
        }
        
        // SET_FAN:1
        if (cmd == BTCommand::SET_FAN) {
            int fan = value.toInt();
            if (fan >= 0 && fan <= 2) {
                if (state_lock()) {
                    g_fanMode = fan;
                    state_unlock();
                }
                SerialBT.println(BTResponse::OK);
                bluetooth_log("Fan mode set to " + String(fan));
            } else {
                SerialBT.println("ERROR:Invalid fan mode");
            }
            return;
        }
        
        // LOAD_PROFILE:test.prof
        if (cmd == BTCommand::LOAD_PROFILE) {
            String profilePath = "/profiles/" + value;
            storage_save_profile_path_nvs(profilePath.c_str());
            
            if (storage_load_profile()) {
                SerialBT.println(BTResponse::OK);
                bluetooth_log("Profile loaded: " + value);
            } else {
                SerialBT.println("ERROR:Failed to load profile");
            }
            return;
        }
    }
    
    // Nieznana komenda
    SerialBT.println("ERROR:Unknown command");
}

void bluetooth_handle_communication() {
    // Sprawdź połączenie
    static bool wasConnected = false;
    bool isConnected = bluetooth_is_connected();
    
    if (isConnected && !wasConnected) {
        log_msg(LOG_LEVEL_INFO, "Bluetooth client connected");
        SerialBT.println("WELCOME:Wedzarnia ESP32 v3.4");
        bluetooth_send_status();
    } else if (!isConnected && wasConnected) {
        log_msg(LOG_LEVEL_INFO, "Bluetooth client disconnected");
    }
    wasConnected = isConnected;
    
    // Odbierz komendy
    while (SerialBT.available()) {
        char c = SerialBT.read();
        
        if (c == '\n' || c == '\r') {
            if (btCommandBuffer.length() > 0) {
                bluetooth_parse_command(btCommandBuffer);
                btCommandBuffer = "";
            }
        } else {
            btCommandBuffer += c;
            
            // Zabezpieczenie przed przepełnieniem bufora
            if (btCommandBuffer.length() > 128) {
                btCommandBuffer = "";
                SerialBT.println("ERROR:Command too long");
            }
        }
    }
    
    // Automatyczne wysyłanie statusu co 2 sekundy
    if (isConnected && (millis() - lastStatusSend > STATUS_SEND_INTERVAL)) {
        bluetooth_send_status();
        lastStatusSend = millis();
    }
}
