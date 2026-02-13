# Poprawki stabilności - Wędzarnia ESP32

## Podsumowanie zmian

Wszystkie poprawione pliki znajdują się w katalogu `esp32_fixed/`. Skopiuj je do swojego projektu Arduino IDE zastępując oryginalne pliki.

## Lista zmian po plikach

### 🔧 config.h
- `WDT_TIMEOUT`: 8 → **10 sekund** (uwzględnia czas konwersji DS18B20)
- `TASK_WATCHDOG_TIMEOUT`: 2000 → **10000 ms** (eliminacja fałszywych alarmów)
- **Nowe makro `LOG_FMT()`** - bezpieczne logowanie przez `snprintf` zamiast `String`
- `log_msg()` zachowuje kompatybilność wsteczną ale preferowana jest wersja `const char*`

### 🔧 outputs.cpp
- **Sprawdzanie wyniku `output_lock()` / `heater_lock()`** we WSZYSTKICH funkcjach:
  - `allOutputsOff()` - loguje błąd ale wyłącza wyjścia (bezpieczeństwo)
  - `initHeaterEnable()` - return przy braku locka
  - `applySoftEnable()` - return przy braku locka
  - `areHeatersReady()` - return false przy braku locka
  - `mapPowerToHeaters()` - return przy braku locka
  - `handleFanLogic()` - return przy braku locka

### 🔧 process.cpp — KRYTYCZNE
- **`g_currentStep++`** w `handleAutoMode()` - chroniony `state_lock()` ✅
- **`g_currentStep = nextStep`** w `process_force_next_step()` - wewnątrz locka ✅
- **`g_currentStep = 0`** w `process_start_auto()` - chroniony lockiem ✅
- **`g_stepStartTime = millis()`** w `applyCurrentStep()` - wewnątrz locka ✅
- **Kopia danych kroku** (`memcpy`) w `handleAutoMode()` - bezpieczny odczyt ✅
- **`g_processStartTime`** odczytywany pod lockiem w `process_run_control_logic()` ✅
- Zamiana `String` logowania na `LOG_FMT()`

### 🔧 hardware.cpp
- **NAPRAWIONY BUG `logToFile()`**: było `if (logFile = SD.open(...))` (przypisanie!), poprawione na osobną zmienną
- **`shouldEnterLowPower()`**: dodane sprawdzenie `if (!state_lock()) return false`
- **`deleteOldestLog()`**: zamiana `String` na `char[]` dla nazwy pliku
- Zamiana ~15 wywołań `log_msg` z `String` na `LOG_FMT` z `snprintf`

### 🔧 sensors.cpp
- **`readTempWithTimeout()` uproszczony**: konwersja już zakończona, wystarczy 1 odczyt + retry na 85.0 (power-on reset)
- Zamiana `String` logowania na `LOG_FMT()` w `identifyAndAssignSensors()`, `reassignSensors()`, `readTemperature()`
- `getSensorAssignmentInfo()` - `snprintf` zamiast konkatenacji String

### 🔧 tasks.cpp
- **Zwiększony stos `taskSensors`**: 4096 → **5120** bajtów (DS18B20 + NVS)
- Zamiana WSZYSTKICH `String` logów na `LOG_FMT()`
- `getTaskWatchdogStatus()` - budowanie przez `snprintf`
- Użycie `HEAP_WARNING_THRESHOLD` zamiast magic number

### 🔧 wifimanager.cpp
- Zamiana `WiFi.softAPIP().toString()` na `snprintf` z tablicą `char`
- Zamiana `WiFi.localIP().toString()` na `snprintf`
- Zamiana WSZYSTKICH `String` logów na `LOG_FMT()`

### 🔧 storage.cpp
- **`storage_load_profile()`**: `strncmp()` zamiast `String(path).startsWith()` 
- **`storage_list_profiles_json()`**: `snprintf` w stałym buforze `char[512]` zamiast `String` concat
- **`storage_get_profile_as_json()`**: `snprintf` w buforze `char[2048]` zamiast `String` concat
- **`storage_list_github_profiles_json()`**: `snprintf` zamiast `String` concat
- **`storage_load_github_profile()`**: URL budowany przez `snprintf`
- **`storage_backup_config()`**: ścieżka przez `snprintf`
- **`cleanupOldBackups()`**: `char[][]` + bubble sort zamiast `vector<String>` + `std::sort`
- **`storage_list_backups_json()`**: `snprintf` zamiast `String` concat
- Zabezpieczenie przed przepełnieniem buforów JSON

## Pliki NIE zmienione (nie wymagały poprawek)
- `state.cpp` / `state.h` - już poprawne
- `outputs.h` / `process.h` / `sensors.h` / `hardware.h` / `tasks.h` / `storage.h` / `wifimanager.h` - nagłówki bez zmian
- `ui.cpp` / `web_server.cpp` - duże pliki, String w UI/web jest mniej krytyczny (jednorazowe requesty, nie pętla ciągła). Zalecane jako **następny krok optymalizacji**.

## Co NIE zostało zmienione (zgodnie z wymaganiem)
- **Brak auto-recovery po przegrzaniu** - `PAUSE_OVERHEAT` pozostaje permanentny, wymaga ręcznego restartu
