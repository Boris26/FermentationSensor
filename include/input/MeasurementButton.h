#pragma once

#include <Arduino.h>

class MeasurementButton
{
public:
    explicit MeasurementButton(uint8_t pin);

    void begin();
    void update();

    bool wasPressed();

private:
    static constexpr unsigned long DEBOUNCE_TIME_MS = 50;

    uint8_t _pin;

    bool _lastReading = HIGH;
    bool _stableState = HIGH;
    bool _pressed = false;

    unsigned long _lastDebounceTime = 0;
};