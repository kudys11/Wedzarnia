// bluetooth.cpp - Implementacja BLE z NimBLE (ESP32-S3)
#include "bluetooth.h"
#include "config.h"
#include "state.h"
#include "process.h"
#include "storage.h"
#include <SD.h>

// ======================================================
// Zmienne wewnętrzne
// ======================================================
static NimBLEServer*         pServer        = nullptr;
static NimBLECharacteristic* pStatusChar    = nullptr; // notify → status co 2s
static NimBLECharacteristic* pCommandChar   = nullptr; // write  ← komendy
static NimBLECharacteristic* pResponseChar  = nullptr; // notify ← odpowiedzi

static bool    deviceConnected    = false;
static bool    prevConnected      = false;
static unsigned long lastStatusMs = 0;
constexpr unsigned long STATUS_INTERVAL_MS = 2000;

// Kolejka komend (thread-safe: zapisywana z callbacku BLE, czytana w loop())
static volatile bool     commandPending = false;
static char              commandBuf[128] = {0};
static portMUX_TYPE      commandMux = portMUX_INITIALIZER_UNLOCKED;

// ======================================================
// Callbacki serwera BLE
// ======================================================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo) override {
        deviceConnected = true;
        log_msg(LOG_LEVEL_INFO, "BLE: Client connected");
        // Zmniejsz interval połączenia dla szybszych powiadomień
        pSrv->updateConnParams(connInfo.getConnHandle(), 6, 12, 0, 200);
    }

    void onDisconnect(NimBLEServer* pSrv, NimBLEConnInfo& connInfo, int reason) override {
        deviceConnected = false;
        log_msg(LOG_LEVEL_INFO, "BLE: Client disconnected, restarting advertising...");
        NimBLEDevice::startAdvertising();
    }
};

// ======================================================
// Callback odbioru komend z telefonu
// Uruchamiany w wątku BLE - tylko kopiuj do bufora!
// ======================================================
class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        std::string val = pChar->getValue();
        if (val.length() == 0 || val.length() >= sizeof(commandBuf)) return;

        portENTER_CRITICAL(&commandMux);
        if (!commandPending) {                          // Nie nadpisuj poprzedniej
            memcpy(commandBuf, val.c_str(), val.length());
            commandBuf[val.length()] = '\0';
            commandPending = true;
        }
        portEXIT_CRITICAL(&commandMux);
    }
};

// ======================================================
// Inicjalizacja BLE
// ======================================================
void ble_init() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);            // Maksymalna moc ~+9dBm

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Utwórz serwis
    NimBLEService* pService = pServer->createService(BLE_SERVICE_UUID);

    // Charakterystyka STATUS - notify, tylko odczyt przez telefon
    pStatusChar = pService->createCharacteristic(
        BLE_CHAR_STATUS_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Charakterystyka COMMAND - write, telefon wysyła komendy
    pCommandChar = pService->createCharacteristic(
        BLE_CHAR_COMMAND_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pCommandChar->setCallbacks(new CommandCallbacks());

    // Charakterystyka RESPONSE - notify, odpowiedzi na komendy
    pResponseChar = pService->createCharacteristic(
        BLE_CHAR_RESPONSE_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    pService->start();

    // Reklama BLE
    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(BLE_SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x06);
    pAdv->setMaxPreferred(0x12);
    NimBLEDevice::startAdvertising();

    log_msg(LOG_LEVEL_INFO, "BLE started: '" BLE_DEVICE_NAME "'");
    log_msg(LOG_LEVEL_INFO, "BLE: Waiting for connection...");
}

// ======================================================
// Sprawdź czy telefon jest podłączony
// ======================================================
bool ble_is_connected() {
    return deviceConnected;
}

// ======================================================
// Wyślij krótką odpowiedź / log do telefonu
// ======================================================
void ble_notify(const String& message) {
    if (!deviceConnected || !pResponseChar) return;
    pResponseChar->setValue(message.c_str());
    pResponseChar->notify();
}

// ======================================================
// Buduj i wyślij pełny status
// FORMAT: STATUS:state,tSet,tChamber,tMeat,power,smoke,fan,step,total,door,errSensor,errHeat
// ======================================================
void ble_send_status() {
    if (!deviceConnected || !pStatusChar) return;

    char buf[192];

    if (!state_lock()) return;

    const char* stateStr;
    switch (g_currentState) {
        case ProcessState::IDLE:           stateStr = "IDLE";          break;
        case ProcessState::RUNNING_AUTO:   stateStr = "AUTO";          break;
        case ProcessState::RUNNING_MANUAL: stateStr = "MANUAL";        break;
        case ProcessState::PAUSE_DOOR:     stateStr = "PAUSE_DOOR";    break;
        case ProcessState::PAUSE_SENSOR:   stateStr = "PAUSE_SENSOR";  break;
        case ProcessState::PAUSE_OVERHEAT: stateStr = "PAUSE_HEAT";    break;
        case ProcessState::PAUSE_USER:     stateStr = "PAUSE_USER";    break;
        case ProcessState::ERROR_PROFILE:  stateStr = "ERROR";         break;
        case ProcessState::SOFT_RESUME:    stateStr = "RESUMING";      break;
        default:                           stateStr = "UNKNOWN";       break;
    }

    snprintf(buf, sizeof(buf),
        "STATUS:%s,%.1f,%.1f,%.1f,%d,%d,%d,%d,%d,%d,%d,%d",
        stateStr,
        (double)g_tSet,
        (double)g_tChamber,
        (double)g_tMeat,
        (int)g_powerMode,
        (int)g_manualSmokePwm,
        (int)g_fanMode,
        (int)g_currentStep,
        (int)g_stepCount,
        g_doorOpen     ? 1 : 0,
        g_errorSensor  ? 1 : 0,
        g_errorOverheat? 1 : 0
    );

    state_unlock();

    pStatusChar->setValue(buf);
    pStatusChar->notify();
}

// ======================================================
// Wyślij listę profili z SD
// ======================================================
static void sendProfileList() {
    ble_notify("PROFILES_START");

    File root = SD.open("/profiles");
    if (!root || !root.isDirectory()) {
        ble_notify("PROFILES_END");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".prof")) {
                ble_notify("PROFILE:" + name);
                delay(20); // Mały odstęp żeby BLE nie tracił pakietów
            }
        }
        file = root.openNextFile();
    }
    root.close();
    ble_notify("PROFILES_END");
}

// ======================================================
// Parsowanie i wykonanie komendy
// Wywołane z loop() - bezpieczne!
// ======================================================
static void executeCommand(const char* cmd) {
    String command = String(cmd);
    command.trim();
    if (command.length() == 0) return;

    log_msg(LOG_LEVEL_INFO, "BLE cmd: " + command);

    // --- Proste komendy bez parametrów ---

    if (command == BTCommand::PING) {
        ble_notify("PONG");
        return;
    }

    if (command == BTCommand::GET_STATUS) {
        ble_send_status();
        return;
    }

    if (command == BTCommand::GET_TEMPS) {
        char buf[64];
        if (!state_lock()) return;
        snprintf(buf, sizeof(buf), "TEMPS:%.2f,%.2f,%.1f",
            (double)g_tChamber, (double)g_tMeat, (double)g_tSet);
        state_unlock();
        ble_notify(buf);
        return;
    }

    if (command == BTCommand::GET_PROFILES) {
        sendProfileList();
        return;
    }

    if (command == BTCommand::START_AUTO) {
        if (!state_lock()) return;
        if (g_errorProfile) {
            state_unlock();
            ble_notify("ERROR:No profile loaded");
            return;
        }
        state_unlock();
        process_start_auto();
        ble_notify("OK");
        return;
    }

    if (command == BTCommand::START_MANUAL) {
        process_start_manual();
        ble_notify("OK");
        return;
    }

    if (command == BTCommand::STOP) {
        if (state_lock()) {
            g_currentState = ProcessState::PAUSE_USER;
            state_unlock();
        }
        ble_notify("OK");
        return;
    }

    if (command == BTCommand::PAUSE) {
        if (!state_lock()) return;
        if (g_currentState == ProcessState::RUNNING_AUTO ||
            g_currentState == ProcessState::RUNNING_MANUAL) {
            g_currentState = ProcessState::PAUSE_USER;
            state_unlock();
            ble_notify("OK");
        } else {
            state_unlock();
            ble_notify("ERROR:Not running");
        }
        return;
    }

    if (command == BTCommand::RESUME) {
        if (!state_lock()) return;
        if (g_currentState == ProcessState::PAUSE_USER) {
            state_unlock();
            process_resume();
            ble_notify("OK");
        } else {
            state_unlock();
            ble_notify("ERROR:Not paused");
        }
        return;
    }

    if (command == BTCommand::NEXT_STEP) {
        process_force_next_step();
        ble_notify("OK");
        return;
    }

    // --- Komendy z parametrem: KOMENDA:wartość ---
    int colonIdx = command.indexOf(':');
    if (colonIdx > 0) {
        String cmd2  = command.substring(0, colonIdx);
        String value = command.substring(colonIdx + 1);

        if (cmd2 == BTCommand::SET_TEMP) {
            double temp = value.toDouble();
            if (temp >= CFG_T_MIN_SET && temp <= CFG_T_MAX_SET) {
                if (state_lock()) { g_tSet = temp; state_unlock(); }
                ble_notify("OK");
            } else {
                ble_notify("ERROR:Temp out of range");
            }
            return;
        }

        if (cmd2 == BTCommand::SET_POWER) {
            int pw = value.toInt();
            if (pw >= CFG_POWERMODE_MIN && pw <= CFG_POWERMODE_MAX) {
                if (state_lock()) { g_powerMode = pw; state_unlock(); }
                ble_notify("OK");
            } else {
                ble_notify("ERROR:Power 1-3");
            }
            return;
        }

        if (cmd2 == BTCommand::SET_SMOKE) {
            int sm = value.toInt();
            if (sm >= CFG_SMOKE_PWM_MIN && sm <= CFG_SMOKE_PWM_MAX) {
                if (state_lock()) { g_manualSmokePwm = sm; state_unlock(); }
                ble_notify("OK");
            } else {
                ble_notify("ERROR:Smoke 0-255");
            }
            return;
        }

        if (cmd2 == BTCommand::SET_FAN) {
            int fm = value.toInt();
            if (fm >= 0 && fm <= 2) {
                if (state_lock()) { g_fanMode = fm; state_unlock(); }
                ble_notify("OK");
            } else {
                ble_notify("ERROR:Fan 0-2");
            }
            return;
        }

        if (cmd2 == BTCommand::LOAD_PROFILE) {
            String path = "/profiles/" + value;
            storage_save_profile_path_nvs(path.c_str());
            if (storage_load_profile()) {
                ble_notify("OK");
            } else {
                ble_notify("ERROR:Profile load failed");
            }
            return;
        }
    }

    ble_notify("ERROR:Unknown command");
}

// ======================================================
// Główna pętla - wywołuj w loop()
// ======================================================
void ble_handle() {
    // 1. Obsługa zdarzenia połączenia/rozłączenia
    if (deviceConnected && !prevConnected) {
        prevConnected = true;
        // Powitanie i natychmiastowy status
        ble_notify("WELCOME:Wedzarnia S3 v3.5 BLE");
        delay(50);
        ble_send_status();
        lastStatusMs = millis();
    }
    if (!deviceConnected && prevConnected) {
        prevConnected = false;
    }

    // 2. Wykonaj oczekującą komendę
    portENTER_CRITICAL(&commandMux);
    bool hasPending = commandPending;
    char localCmd[128] = {0};
    if (hasPending) {
        memcpy(localCmd, commandBuf, sizeof(localCmd));
        commandPending = false;
    }
    portEXIT_CRITICAL(&commandMux);

    if (hasPending) {
        executeCommand(localCmd);
    }

    // 3. Automatyczny status co 2 sekundy
    if (deviceConnected && (millis() - lastStatusMs >= STATUS_INTERVAL_MS)) {
        ble_send_status();
        lastStatusMs = millis();
    }
}
