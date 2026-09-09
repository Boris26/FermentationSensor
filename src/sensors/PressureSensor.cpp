#include "sensors/PressureSensor.h"

#include <Arduino.h>
#include <DFRobot_LWLP.h>


namespace
{
    DFRobot_LWLP lwlp;

    constexpr unsigned long READ_INTERVAL_MS = 500;

    unsigned long lastReadMs = 0;
}


void PressureSensor::begin()
{
    Serial.println(
        "PressureSensor: initializing..."
    );

    const int result =
        lwlp.begin();

    if (result != 0)
    {
        _available = false;

        Serial.print(
            "PressureSensor: initialization failed, error="
        );

        Serial.println(result);

        return;
    }

    _available = true;

    Serial.println(
        "PressureSensor: ready."
    );
}


void PressureSensor::update()
{
    if (!_available)
    {
        return;
    }

    const unsigned long now =
        millis();

    if (
        now - lastReadMs <
        READ_INTERVAL_MS
    )
    {
        return;
    }

    lastReadMs = now;

    const DFRobot_LWLP::sLwlp_t data =
        lwlp.getData();

    _pressurePa =
        data.presure;

    Serial.print(
        "PressureSensor: "
    );

    Serial.print(
        _pressurePa,
        2
    );

    Serial.print(
        " Pa | Sensor temperature: "
    );

    Serial.print(
        data.temperature,
        2
    );

    Serial.println(
        " C"
    );
}


bool PressureSensor::isAvailable() const
{
    return _available;
}


float PressureSensor::getPressurePa() const
{
    return _pressurePa;
}