#pragma once

#include <Arduino.h>
#include "output/LedBlinkState.h"

class StatusLed
{
public:
    explicit StatusLed(uint8_t pin);

    void begin();
    void update();

    void on();
    void off();

    void startBlinking(unsigned long intervalMs);
    void stopBlinking();

private:
    uint8_t pin_;
    LedBlinkState state_;
};
