#pragma once

#include <Arduino.h>

struct TemperatureSensorId
{
    static constexpr size_t SIZE = 8;

    uint8_t bytes[SIZE] = {};

    bool isValid() const
    {
        for (size_t i = 0; i < SIZE; ++i) {
            if (bytes[i] != 0) {
                return true;
            }
        }

        return false;
    }

    bool equals(const TemperatureSensorId& other) const
    {
        for (size_t i = 0; i < SIZE; ++i) {
            if (bytes[i] != other.bytes[i]) {
                return false;
            }
        }

        return true;
    }
};