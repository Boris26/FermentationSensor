#pragma once

#include "sensors/TemperatureSensorId.h"
#include "storage/FlashStorage.h"

class TemperatureSensorStore
{
public:
    explicit TemperatureSensorStore(
        FlashStorage& storage
    );

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

    bool clearAmbientSensor();
    bool clearBeerSensor();

private:
    bool hasSensor(
        const char* key
    ) const;

    TemperatureSensorId loadSensor(
        const char* key
    ) const;

    bool saveSensor(
        const char* key,
        const TemperatureSensorId& sensorId
    );

    FlashStorage& _storage;

    bool _initialized = false;
};