#pragma once

#include <Arduino.h>

class PauseButton
{
public:
    explicit PauseButton(uint8_t pin);

    void begin();
    void update();

    bool isPaused() const;

private:
    static constexpr unsigned long DEBOUNCE_TIME_MS = 50;

    uint8_t _pin;

    bool _paused = false;

    bool _lastReading = HIGH;
    bool _stableState = HIGH;

    unsigned long _lastDebounceTime = 0;
};