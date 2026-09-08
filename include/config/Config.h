#pragma once

#include <Arduino.h>

constexpr char DEVICE_ID[] = "FERM-01";

constexpr unsigned long SERIAL_BAUD_RATE = 115200;

constexpr uint8_t ONE_WIRE_PIN = 4;
constexpr uint8_t MEASUREMENT_BUTTON_PIN = 3;

constexpr uint8_t STATUS_LED_PIN = 2;
constexpr uint8_t SENSOR_ERROR_LED_PIN = 5;
constexpr uint8_t SESSION_LED_PIN = 6;

constexpr unsigned long TEMPERATURE_INTERVAL_MS = 60000;
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 60000;

constexpr unsigned long SENSOR_CHECK_INTERVAL_MS = 1000;

constexpr unsigned long STATUS_LED_BLINK_INTERVAL_MS = 500;
constexpr unsigned long SESSION_LED_BLINK_INTERVAL_MS = 1000;
constexpr unsigned long ERROR_LED_BLINK_INTERVAL_MS = 500;