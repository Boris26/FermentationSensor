#include "output/ErrorLed.h"

ErrorLed::ErrorLed(uint8_t pin)
    : pin_(pin),
      ledState_(false),
      blinking_(false),
      blinkIntervalMs_(500),
      lastToggleMs_(0)
{
}

void ErrorLed::begin()
{
    pinMode(pin_, OUTPUT);
    off();
}

void ErrorLed::update()
{
    if (!blinking_) {
        return;
    }

    const unsigned long now = millis();

    if (now - lastToggleMs_ >= blinkIntervalMs_) {
        lastToggleMs_ = now;

        ledState_ = !ledState_;
        digitalWrite(pin_, ledState_ ? HIGH : LOW);
    }
}

void ErrorLed::on()
{
    blinking_ = false;
    ledState_ = true;

    digitalWrite(pin_, HIGH);
}

void ErrorLed::off()
{
    blinking_ = false;
    ledState_ = false;

    digitalWrite(pin_, LOW);
}

void ErrorLed::startBlinking(unsigned long intervalMs)
{
    blinkIntervalMs_ = intervalMs;
    blinking_ = true;
    lastToggleMs_ = millis();
}

void ErrorLed::stopBlinking()
{
    blinking_ = false;
    off();
}