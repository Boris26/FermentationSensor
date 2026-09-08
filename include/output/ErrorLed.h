#pragma once

#include <Arduino.h>

class ErrorLed
{
public:
    explicit ErrorLed(uint8_t pin);

    void begin();
    void update();

    void on();
    void off();

    void startBlinking(unsigned long intervalMs);
    void stopBlinking();

private:
    uint8_t pin_;

    bool ledState_;
    bool blinking_;

    unsigned long blinkIntervalMs_;
    unsigned long lastToggleMs_;
};