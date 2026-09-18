#pragma once

#include <stdint.h>

// Hardware-independent blink state.  Passing the clock value in makes the
// rollover behaviour testable and keeps all elapsed-time checks unsigned.
class LedBlinkState
{
public:
    bool update(uint32_t nowMs)
    {
        if (!_blinking || nowMs - _lastToggleMs < _intervalMs) return false;

        _lastToggleMs = nowMs;
        _outputOn = !_outputOn;
        return true;
    }

    void on()
    {
        _blinking = false;
        _outputOn = true;
    }

    void off()
    {
        _blinking = false;
        _outputOn = false;
    }

    void startBlinking(uint32_t intervalMs, uint32_t nowMs)
    {
        _intervalMs = intervalMs;
        _lastToggleMs = nowMs;
        _blinking = true;
        _outputOn = true;
    }

    bool outputOn() const { return _outputOn; }
    bool isBlinking() const { return _blinking; }
    uint32_t lastToggleMs() const { return _lastToggleMs; }

private:
    bool _outputOn = false;
    bool _blinking = false;
    uint32_t _intervalMs = 500;
    uint32_t _lastToggleMs = 0;
};
