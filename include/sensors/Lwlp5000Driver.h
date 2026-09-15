#pragma once

#include <stdint.h>
#include <Wire.h>

enum class Lwlp5000ReadError : uint8_t
{
    NONE = 0,
    NOT_INITIALIZED,
    COMMAND_FAILED,
    SHORT_READ,
    INCOMPLETE_READ
};

struct Lwlp5000Sample
{
    float pressurePa = 0.0f;
    float temperatureC = 0.0f;
    uint8_t status = 0;
    bool valid = false;
    Lwlp5000ReadError error = Lwlp5000ReadError::NONE;
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

    static constexpr float PRESSURE_MIN_PA = -500.0f;
    static constexpr float PRESSURE_MAX_PA = 500.0f;
    static constexpr float PRESSURE_SPAN_PA = PRESSURE_MAX_PA - PRESSURE_MIN_PA;
    static constexpr float PRESSURE_RAW_RANGE = 16384.0f;

    static constexpr float TEMPERATURE_MIN_C = -40.0f;
    static constexpr float TEMPERATURE_SPAN_C = 125.0f;
    static constexpr float TEMPERATURE_RAW_RANGE = 65536.0f;

    static uint16_t decodeTemperatureRaw(uint8_t high, uint8_t middle);
    static float temperatureRawToC(uint16_t temperatureRaw);
    bool sendMeasurementCommand();

    TwoWire& _wire;
    bool _initialized = false;
};
