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
    struct StoredTemperatureConfiguration
    {
        bool hasAmbient = false;
        bool hasBeer = false;

        uint8_t ambient[
            TemperatureSensorId::SIZE
        ] = {};

        uint8_t beer[
            TemperatureSensorId::SIZE
        ] = {};
    };

    bool loadConfiguration(
        StoredTemperatureConfiguration& configuration
    ) const;

    bool saveConfiguration(
        const StoredTemperatureConfiguration& configuration
    );

    TemperatureSensorId toSensorId(
        const uint8_t* bytes
    ) const;

    FlashStorage& _storage;

    bool _initialized = false;
};