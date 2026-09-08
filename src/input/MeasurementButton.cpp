#include "input/MeasurementButton.h"

MeasurementButton::MeasurementButton(uint8_t pin)
    : _pin(pin)
{
}

void MeasurementButton::begin()
{
    pinMode(_pin, INPUT_PULLUP);

    _lastReading = digitalRead(_pin);
    _stableState = _lastReading;

    Serial.println("MeasurementButton: initialized.");
}

void MeasurementButton::update()
{
    const bool reading =
        digitalRead(_pin);

    if (reading != _lastReading) {
        _lastDebounceTime = millis();
        _lastReading = reading;
    }

    if (
        millis() - _lastDebounceTime <
        DEBOUNCE_TIME_MS
    ) {
        return;
    }

    if (reading == _stableState) {
        return;
    }

    _stableState = reading;

    if (_stableState == LOW) {
        _pressed = true;
    }
}

bool MeasurementButton::wasPressed()
{
    if (!_pressed) {
        return false;
    }

    _pressed = false;

    return true;
}