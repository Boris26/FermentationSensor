#include "output/StatusLed.h"

StatusLed::StatusLed(uint8_t pin)
    : pin_(pin)
{
}

void StatusLed::begin()
{
    pinMode(pin_, OUTPUT);
    off();
}

void StatusLed::update()
{
    if (state_.update(static_cast<uint32_t>(millis()))) {
        digitalWrite(pin_, state_.outputOn() ? HIGH : LOW);
    }
}

void StatusLed::on()
{
    state_.on();
    digitalWrite(pin_, HIGH);
}

void StatusLed::off()
{
    state_.off();
    digitalWrite(pin_, LOW);
}

void StatusLed::startBlinking(unsigned long intervalMs)
{
    state_.startBlinking(
        static_cast<uint32_t>(intervalMs), static_cast<uint32_t>(millis())
    );
    digitalWrite(pin_, HIGH);

}

void StatusLed::stopBlinking()
{
    off();
}
