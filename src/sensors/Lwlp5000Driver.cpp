#include "sensors/Lwlp5000Driver.h"

#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t Lwlp5000Driver::MEASUREMENT_COMMAND[
    Lwlp5000Driver::MEASUREMENT_COMMAND_SIZE
];
constexpr float Lwlp5000Driver::PRESSURE_MIN_PA;
constexpr float Lwlp5000Driver::PRESSURE_MAX_PA;
constexpr float Lwlp5000Driver::PRESSURE_SPAN_PA;
constexpr float Lwlp5000Driver::PRESSURE_RAW_RANGE;
constexpr float Lwlp5000Driver::TEMPERATURE_MIN_C;
constexpr float Lwlp5000Driver::TEMPERATURE_SPAN_C;
constexpr float Lwlp5000Driver::TEMPERATURE_RAW_RANGE;

Lwlp5000Driver::Lwlp5000Driver(TwoWire& wire) : _wire(wire) {}

bool Lwlp5000Driver::begin()
{
    _wire.begin();
    _wire.beginTransmission(I2C_ADDRESS);
    _initialized = _wire.endTransmission() == 0;
    return _initialized;
}

Lwlp5000Sample Lwlp5000Driver::read()
{
    Lwlp5000Sample sample;
    if (!_initialized) {
        sample.error = Lwlp5000ReadError::NOT_INITIALIZED;
        return sample;
    }
    if (!sendMeasurementCommand()) {
        sample.error = Lwlp5000ReadError::COMMAND_FAILED;
        return sample;
    }

    // The command-mode conversion needs at least 30 ms. There are no retries
    // or extra filter samples, so a read still fits in the 100 ms cadence.
    delay(CONVERSION_TIME_MS);

    const uint8_t received = _wire.requestFrom(I2C_ADDRESS, RESPONSE_SIZE);
    if (received != RESPONSE_SIZE) {
        while (_wire.available()) (void) _wire.read();
        sample.error = Lwlp5000ReadError::SHORT_READ;
        return sample;
    }

    uint8_t response[RESPONSE_SIZE];
    for (uint8_t index = 0; index < RESPONSE_SIZE; ++index) {
        if (!_wire.available()) {
            sample.error = Lwlp5000ReadError::INCOMPLETE_READ;
            return sample;
        }
        response[index] = static_cast<uint8_t>(_wire.read());
    }

    // The datasheet documents the first byte as a status byte but does not
    // define bit meanings that would justify rejecting otherwise complete
    // samples. Preserve it for diagnostics without applying guessed masks.
    sample.status = response[0];

    const uint16_t pressureRaw = decodePressureRaw(response[1], response[2]);
    const uint16_t temperatureRaw = decodeTemperatureRaw(response[4], response[5]);
    sample.pressurePa = pressureRawToPa(pressureRaw);
    sample.temperatureC = temperatureRawToC(temperatureRaw);
    sample.valid = true;
    return sample;
}

uint16_t Lwlp5000Driver::decodePressureRaw(uint8_t high, uint8_t middle)
{
    // Datasheet: pressure occupies bits [23:10] of the 24-bit pressure field.
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(high) << 8 | middle) >> 2
    );
}

float Lwlp5000Driver::pressureRawToPa(uint16_t pressureRaw)
{
    return (static_cast<float>(pressureRaw) / PRESSURE_RAW_RANGE) *
        PRESSURE_SPAN_PA + PRESSURE_MIN_PA;
}

uint16_t Lwlp5000Driver::decodeTemperatureRaw(uint8_t high, uint8_t middle)
{
    // Datasheet: temperature occupies bits [23:08]; the last byte is reserved.
    return static_cast<uint16_t>(
        static_cast<uint16_t>(high) << 8 | middle
    );
}

float Lwlp5000Driver::temperatureRawToC(uint16_t temperatureRaw)
{
    // Datasheet: T = (125 / 2^16) * T1 - 40, yielding -40 ... +85 C.
    return (static_cast<float>(temperatureRaw) / TEMPERATURE_RAW_RANGE) *
        TEMPERATURE_SPAN_C + TEMPERATURE_MIN_C;
}

bool Lwlp5000Driver::sendMeasurementCommand()
{
    _wire.beginTransmission(I2C_ADDRESS);
    if (_wire.write(MEASUREMENT_COMMAND, MEASUREMENT_COMMAND_SIZE) !=
        MEASUREMENT_COMMAND_SIZE) return false;
    return _wire.endTransmission() == 0;
}
