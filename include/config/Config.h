#pragma once

#include <Arduino.h>

constexpr char DEVICE_ID[] = "FERM-01";

constexpr unsigned long SERIAL_BAUD_RATE = 115200;

constexpr uint8_t ONE_WIRE_PIN = 4;
constexpr uint8_t PAUSE_BUTTON_PIN = 3;
constexpr uint8_t STATUS_LED_PIN = 2;
constexpr uint8_t SENSOR_ERROR_LED_PIN = 5;

constexpr unsigned long TEMPERATURE_INTERVAL_MS = 60000;
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 60000;
