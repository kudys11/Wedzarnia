// Wedzarnia.ino - Główny plik projektu
#include "config.h"
#include "state.h"
#include "hardware.h"
#include "storage.h"
#include "web_server.h"
#include "tasks.h"
#include "outputs.h"
#include "ui.h"
#include <esp_task_wdt.h>

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== WEDZARNIA ESP32 v3.0 (Modular) ===\n");
    // 1. NVS musi być pierwsze
    nvs_init();
    
    // 2. Inicjalizacja mutexów stanu
    init_state();
    
    // 3. Piny GPIO (z przyciskami zamiast enkodera)
    hardware_init_pins();

    // 4. Inicjalizacja całego UI (zamiast tylko enkodera)
    ui_init(); // <--- ZMIANA TUTAJ
   
    // 4. PWM/LEDC
    hardware_init_ledc();

    esp_task_wdt_reset(); // <--- GŁASZCZEMY PIESKA po pierwszych szybkich operacjach
    
    // 5. Wyświetlacz
    hardware_init_display();

    esp_task_wdt_reset(); // <--- GŁASZCZEMY PIESKA po wyświetlaczu
    
    // 6. Czujniki temperatury
    hardware_init_sensors();
    
    // 7. Karta SD
    hardware_init_sd();

    esp_task_wdt_reset(); // <--- GŁASZCZEMY PIESKA po SD

    // 8. Wczytaj konfigurację z NVS
    storage_load_config_nvs();
    
    // 9. Serwer WWW i WiFi
    web_server_init();

    esp_task_wdt_reset(); // <--- OSTATNIE GŁASKANIE przed uruchomieniem zadań
    
    // Sygnał dźwiękowy - gotowe
    buzzerBeep(1, 100, 100);
    Serial.println("\n=== SETUP COMPLETE, STARTING TASKS ===\n");
    // 11. Uruchom zadania FreeRTOS
    tasks_create_all();
}
void loop() {
    // Pusta pętla - wszystko dzieje się w zadaniach RTOS
    vTaskDelete(NULL);
}