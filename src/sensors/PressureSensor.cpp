#include "sensors/PressureSensor.h"

#include <Arduino.h>

void PressureSensor::begin()
{
    Serial.println("PressureSensor: initialized (simulation mode).");
}

void PressureSensor::update()
{
    const unsigned long now = millis();    
}