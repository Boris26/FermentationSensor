#pragma once

#include <Arduino.h>

class PressureSensor
{
public:
    void begin();
    void update();

private:
    static constexpr unsigned long UPDATE_INTERVAL_MS = 1000;

    unsigned long _lastUpdateMs = 0;
};