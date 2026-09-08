#pragma once

#include "sensors/TemperatureSensorId.h"

class TemperatureSensorStore
{
public:
    bool begin();

    bool hasAmbientSensor() const;
    bool hasBeerSensor() const;

    TemperatureSensorId loadAmbientSensor() const;
    TemperatureSensorId loadBeerSensor() const;

    bool saveAmbientSensor(
        const TemperatureSensorId& sensorId
    );

    bool saveBeerSensor(
        const TemperatureSensorId& sensorId
    );

private:
    bool hasSensor(const char* key) const;

    TemperatureSensorId loadSensor(
        const char* key
    ) const;

    bool saveSensor(
        const char* key,
        const TemperatureSensorId& sensorId
    );

    bool _initialized = false;
};