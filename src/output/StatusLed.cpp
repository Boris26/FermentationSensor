#include "output/StatusLed.h"

StatusLed::StatusLed(uint8_t pin)
    : pin_(pin),
      ledState_(false),
      blinking_(false),
      blinkIntervalMs_(500),
      lastToggleMs_(0)
{
}

void StatusLed::begin()
{
    pinMode(pin_, OUTPUT);
    off();
}

void StatusLed::update()
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

void StatusLed::on()
{
    blinking_ = false;
    ledState_ = true;

    digitalWrite(pin_, HIGH);
}

void StatusLed::off()
{
    blinking_ = false;
    ledState_ = false;

    digitalWrite(pin_, LOW);
}

void StatusLed::startBlinking(unsigned long intervalMs)
{
    blinkIntervalMs_ = intervalMs;
    blinking_ = true;

    ledState_ = true;
    digitalWrite(pin_, HIGH);

    lastToggleMs_ = millis();
}

void StatusLed::stopBlinking()
{
    blinking_ = false;
    off();
}