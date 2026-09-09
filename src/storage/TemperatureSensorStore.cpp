#include "storage/TemperatureSensorStore.h"

#include <Arduino.h>

namespace
{
constexpr char AMBIENT_SENSOR_KEY[] =
    "temp_ambient";

constexpr char BEER_SENSOR_KEY[] =
    "temp_beer";
}

TemperatureSensorStore::TemperatureSensorStore(
    FlashStorage& storage
)
    : _storage(storage)
{
}

bool TemperatureSensorStore::begin()
{
    _initialized = true;

    Serial.println(
        "TemperatureSensorStore: ready."
    );

    return true;
}

bool TemperatureSensorStore::hasAmbientSensor() const
{
    return hasSensor(
        AMBIENT_SENSOR_KEY
    );
}

bool TemperatureSensorStore::hasBeerSensor() const
{
    return hasSensor(
        BEER_SENSOR_KEY
    );
}

TemperatureSensorId TemperatureSensorStore::loadAmbientSensor() const
{
    return loadSensor(
        AMBIENT_SENSOR_KEY
    );
}

TemperatureSensorId TemperatureSensorStore::loadBeerSensor() const
{
    return loadSensor(
        BEER_SENSOR_KEY
    );
}

bool TemperatureSensorStore::saveAmbientSensor(
    const TemperatureSensorId& sensorId
)
{
    return saveSensor(
        AMBIENT_SENSOR_KEY,
        sensorId
    );
}

bool TemperatureSensorStore::saveBeerSensor(
    const TemperatureSensorId& sensorId
)
{
    return saveSensor(
        BEER_SENSOR_KEY,
        sensorId
    );
}

bool TemperatureSensorStore::clearAmbientSensor()
{
    if (!_initialized) {
        return false;
    }

    return _storage.remove(
        AMBIENT_SENSOR_KEY
    );
}

bool TemperatureSensorStore::clearBeerSensor()
{
    if (!_initialized) {
        return false;
    }

    return _storage.remove(
        BEER_SENSOR_KEY
    );
}

bool TemperatureSensorStore::hasSensor(
    const char* key
) const
{
    if (!_initialized) {
        return false;
    }

    return _storage.exists(
        key
    );
}

TemperatureSensorId TemperatureSensorStore::loadSensor(
    const char* key
) const
{
    TemperatureSensorId sensorId;

    if (!_initialized) {
        Serial.println(
            "TemperatureSensorStore: not initialized."
        );

        return sensorId;
    }

    const bool loaded =
        _storage.getBytes(
            key,
            sensorId.bytes,
            TemperatureSensorId::SIZE
        );

    if (!loaded) {
        return TemperatureSensorId{};
    }

    return sensorId;
}

bool TemperatureSensorStore::saveSensor(
    const char* key,
    const TemperatureSensorId& sensorId
)
{
    if (!_initialized) {
        return false;
    }

    if (!sensorId.isValid()) {
        Serial.println(
            "TemperatureSensorStore: invalid sensor ID."
        );

        return false;
    }

    return _storage.setBytes(
        key,
        sensorId.bytes,
        TemperatureSensorId::SIZE
    );
}