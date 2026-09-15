#pragma once

#include <stdint.h>

class TwoWire;

struct Lwlp5000Sample
{
    float pressurePa = 0.0f;
    float temperatureC = 0.0f;
    uint8_t status = 0;
    bool valid = false;
};

// Minimal command-mode driver for the LWLP5000 used on the SEN0343 board.
// It deliberately applies neither a tare nor any other pressure offset.
class Lwlp5000Driver
{
public:
    explicit Lwlp5000Driver(TwoWire& wire);

    bool begin();
    Lwlp5000Sample read();

    static uint16_t decodePressureRaw(uint8_t high, uint8_t middle);
    static float pressureRawToPa(uint16_t pressureRaw);

private:
    static constexpr uint8_t I2C_ADDRESS = 0x00;
    static constexpr uint8_t MEASUREMENT_COMMAND_SIZE = 3;
    static constexpr uint8_t RESPONSE_SIZE = 7;
    static constexpr uint8_t MEASUREMENT_COMMAND[MEASUREMENT_COMMAND_SIZE] = {
        0xAA, 0x00, 0x80
    };
    static constexpr unsigned long CONVERSION_TIME_MS = 30;

    static constexpr uint8_t STATUS_BUSY_MASK = 0x20;
    static constexpr uint8_t STATUS_MEMORY_ERROR_MASK = 0x04;
    static constexpr uint8_t STATUS_MATH_SATURATION_MASK = 0x01;
    static constexpr uint8_t STATUS_INVALID_MASK =
        STATUS_BUSY_MASK | STATUS_MEMORY_ERROR_MASK | STATUS_MATH_SATURATION_MASK;

    static constexpr float PRESSURE_MIN_PA = -500.0f;
    static constexpr float PRESSURE_MAX_PA = 500.0f;
    static constexpr float PRESSURE_SPAN_PA = PRESSURE_MAX_PA - PRESSURE_MIN_PA;
    static constexpr float PRESSURE_RAW_RANGE = 16384.0f;

    static constexpr float TEMPERATURE_MIN_C = -40.0f;
    static constexpr float TEMPERATURE_SPAN_C = 85.0f;
    static constexpr float TEMPERATURE_RAW_RANGE = 8192.0f;

    static uint16_t decodeTemperatureRaw(
        uint8_t high, uint8_t middle, uint8_t low
    );
    static float temperatureRawToC(uint16_t temperatureRaw);
    bool sendMeasurementCommand();

    TwoWire& _wire;
    bool _initialized = false;
};
