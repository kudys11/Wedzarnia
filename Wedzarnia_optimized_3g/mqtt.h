// mqtt.h - Komunikacja MQTT
#pragma once
#include <Arduino.h>

// Inicjalizacja MQTT
void mqtt_init();

// Pętla MQTT (do wywołania w tasku web)
void mqtt_loop();

// Ponowne połączenie MQTT
bool mqtt_reconnect();

// Publikuj status
void mqtt_publish_status();

// Publikuj informacje o urządzeniu
void mqtt_publish_device_info();

// Publikuj alert
void mqtt_publish_alert(const char* type, const char* message);

// Włącz/wyłącz MQTT
void mqtt_set_enabled(bool enabled);

// Sprawdź czy połączony
bool mqtt_is_connected();

// Publikuj parametry PID
void mqtt_publish_pid_parameters();