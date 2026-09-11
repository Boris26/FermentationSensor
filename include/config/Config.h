#pragma once

#include <Arduino.h>


constexpr char DEVICE_ID[] = "FERM-01";


constexpr unsigned long SERIAL_BAUD_RATE = 115200;


// Pins
constexpr uint8_t ONE_WIRE_PIN = 4;

constexpr uint8_t MEASUREMENT_BUTTON_PIN = 3;

constexpr uint8_t STATUS_LED_PIN = 2;
constexpr uint8_t SENSOR_ERROR_LED_PIN = 5;
constexpr uint8_t SESSION_LED_PIN = 6;


// Temperature
constexpr unsigned long TEMPERATURE_INTERVAL_MS = 60000;
constexpr float TEMPERATURE_SEND_DELTA_C = 1.0f;


// Pressure diagnostics
constexpr unsigned long PRESSURE_SAMPLE_INTERVAL_MS = 100;
constexpr bool PRESSURE_DIAGNOSTICS_ENABLED = true;

// Pressure calibration and technical bubble detection
constexpr unsigned long PRESSURE_CALIBRATION_MS = 300000;
constexpr float BUBBLE_MIN_TRIGGER_DELTA_PA = 0.50f;
constexpr float BUBBLE_NOISE_FACTOR = 5.0f;
constexpr float BUBBLE_RELEASE_FACTOR = 0.40f;
constexpr unsigned long BUBBLE_MIN_DURATION_MS = 100;
constexpr unsigned long BUBBLE_MAX_DURATION_MS = 3000;
constexpr unsigned long BUBBLE_REFRACTORY_MS = 500;
constexpr unsigned long BUBBLE_ACTIVITY_WINDOW_MS = 60000;
constexpr unsigned long BUBBLE_ACTIVITY_ACK_TIMEOUT_MS = 5000;
constexpr float PRESSURE_BASELINE_TRACKING_ALPHA = 0.001f;


// Status / Heartbeat
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 60000;


// Sensor checks
constexpr unsigned long SENSOR_CHECK_INTERVAL_MS = 1000;


// LEDs
constexpr unsigned long INIT_LED_BLINK_INTERVAL_MS = 250;

constexpr unsigned long STATUS_LED_BLINK_INTERVAL_MS = 500;

constexpr unsigned long SESSION_LED_BLINK_INTERVAL_MS = 1000;


// Normal sensor error
constexpr unsigned long ERROR_LED_BLINK_INTERVAL_MS = 100;


// WiFi / network unavailable
constexpr unsigned long NETWORK_ERROR_LED_BLINK_INTERVAL_MS = 500;

// WiFi is available, but the backend connection/registration is not.
constexpr unsigned long BACKEND_ERROR_LED_BLINK_INTERVAL_MS = 1500;

// Gateway lookup/reconnect policy
constexpr uint8_t GATEWAY_REDISCOVERY_FAILURE_THRESHOLD = 3;
constexpr unsigned long GATEWAY_DISCOVERY_RETRY_INTERVAL_MS = 10000;
