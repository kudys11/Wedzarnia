// hardware.cpp - Inicjalizacja sprzętu
#include "hardware.h"
#include "config.h"
#include "state.h"
#include "outputs.h"
#include <SD.h>
#include <nvs_flash.h>
void hardware_init_pins() {
    // Wyjścia
    pinMode(PIN_SSR1, OUTPUT);
    pinMode(PIN_SSR2, OUTPUT);
    pinMode(PIN_SSR3, OUTPUT);
    pinMode(PIN_FAN, OUTPUT);
    pinMode(PIN_SMOKE_FAN, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    
    // Wejścia
    pinMode(PIN_DOOR, INPUT_PULLUP);
    
    // NOWA INICJALIZACJA PRZYCISKÓW
    pinMode(PIN_BTN_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
    pinMode(PIN_BTN_ENTER, INPUT_PULLUP);
    pinMode(PIN_BTN_EXIT, INPUT_PULLUP);
	
}
void hardware_init_ledc() {
    if (!ledcAttach(PIN_SSR1, LEDC_FREQ, LEDC_RESOLUTION)) 
        Serial.println("LEDC SSR1 attach failed!");
    if (!ledcAttach(PIN_SSR2, LEDC_FREQ, LEDC_RESOLUTION)) 
        Serial.println("LEDC SSR2 attach failed!");
    if (!ledcAttach(PIN_SSR3, LEDC_FREQ, LEDC_RESOLUTION)) 
        Serial.println("LEDC SSR3 attach failed!");
    if (!ledcAttach(PIN_SMOKE_FAN, LEDC_FREQ, LEDC_RESOLUTION)) 
        Serial.println("LEDC SMOKE attach failed!");
    
    allOutputsOff();
}
void hardware_init_sensors() {
    sensors.begin();
    sensors.setWaitForConversion(false);
    sensors.setResolution(12);
    Serial.printf("Found %d DS18B20 sensors\n", sensors.getDeviceCount());
}


void hardware_init_display() {
    display.initR(INITR_BLACKTAB);
    display.setRotation(0);
    display.fillScreen(ST77XX_BLACK);
    display.setCursor(10, 20);
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(2);
    display.println("WEDZARNIA");
    display.setTextSize(1);
    display.println("\n   v3.0 by Wojtek");
    display.println("\n\n   Uruchamianie...");
    delay(2000);
}
void hardware_init_sd() {
    // Dodaj to opóźnienie, aby dać karcie czas na "obudzenie się"
    delay(200);
    
    if (!SD.begin(PIN_SD_CS)) {
        state_lock();
        g_errorProfile = true;
        state_unlock();
        Serial.println("SD card error - manual mode only");
        buzzerBeep(3, 200, 200);
    } else {
        Serial.println("SD card OK");
    }
}
void nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}
