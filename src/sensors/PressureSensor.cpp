#include "sensors/PressureSensor.h"

#include <Arduino.h>
#include <DFRobot_LWLP.h>

#include "config/Config.h"


namespace
{
    DFRobot_LWLP lwlp;

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
        PRESSURE_SAMPLE_INTERVAL_MS
    )
    {
        return;
    }

    lastReadMs = now;

    const DFRobot_LWLP::sLwlp_t data =
        lwlp.getData();

    _pressurePa =
        data.presure;

    if (PRESSURE_DIAGNOSTICS_ENABLED)
    {
        Serial.print("PRESSURE,");
        Serial.print(now);
        Serial.print(',');
        Serial.println(_pressurePa, 2);
    }
}


bool PressureSensor::isAvailable() const
{
    return _available;
}


float PressureSensor::getPressurePa() const
{
    return _pressurePa;
}
