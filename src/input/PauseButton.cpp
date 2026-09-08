#include "input/PauseButton.h"

#include <Arduino.h>

PauseButton::PauseButton(uint8_t pin)
    : _pin(pin)
{
}

void PauseButton::begin()
{
    pinMode(_pin, INPUT_PULLUP);

    _lastReading = digitalRead(_pin);
    _stableState = _lastReading;

    Serial.println("PauseButton: initialized.");
}

void PauseButton::update()
{
    const bool reading = digitalRead(_pin);

    if (reading != _lastReading) {
        _lastDebounceTime = millis();
        _lastReading = reading;
    }

    if (millis() - _lastDebounceTime < DEBOUNCE_TIME_MS) {
        return;
    }

    if (reading == _stableState) {
        return;
    }

    _stableState = reading;

    // Bei INPUT_PULLUP bedeutet LOW: Taster gedrückt
    if (_stableState == LOW) {
        _paused = !_paused;
    }
}

bool PauseButton::isPaused() const
{
    return _paused;
}