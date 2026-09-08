#include "sensors/TemperatureSensorStore.h"

#include <Arduino.h>
#include <kvstore_global_api.h>

namespace
{
constexpr char AMBIENT_SENSOR_KEY[] =
    "temp_ambient";

constexpr char BEER_SENSOR_KEY[] =
    "temp_beer";

constexpr int KV_SUCCESS = 0;
}

bool TemperatureSensorStore::begin()
{
    _initialized = true;

    Serial.println("TemperatureSensorStore: ready.");

    return true;
}

bool TemperatureSensorStore::hasAmbientSensor() const
{
    return hasSensor(AMBIENT_SENSOR_KEY);
}

bool TemperatureSensorStore::hasBeerSensor() const
{
    return hasSensor(BEER_SENSOR_KEY);
}

TemperatureSensorId
TemperatureSensorStore::loadAmbientSensor() const
{
    return loadSensor(AMBIENT_SENSOR_KEY);
}

TemperatureSensorId
TemperatureSensorStore::loadBeerSensor() const
{
    return loadSensor(BEER_SENSOR_KEY);
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

bool TemperatureSensorStore::hasSensor(
    const char* key
) const
{
    if (!_initialized) {
        return false;
    }

    kv_info_t info;

    const int result =
        kv_get_info(key, &info);

    return result == KV_SUCCESS &&
           info.size == TemperatureSensorId::SIZE;
}

TemperatureSensorId
TemperatureSensorStore::loadSensor(
    const char* key
) const
{
    TemperatureSensorId sensorId;

    if (!_initialized) {
        return sensorId;
    }

    size_t actualSize = 0;

    const int result = kv_get(
        key,
        sensorId.bytes,
        TemperatureSensorId::SIZE,
        &actualSize
    );

    if (
        result != KV_SUCCESS ||
        actualSize != TemperatureSensorId::SIZE
    ) {
        return TemperatureSensorId{};
    }

    return sensorId;
}

bool TemperatureSensorStore::saveSensor(
    const char* key,
    const TemperatureSensorId& sensorId
)
{
    if (!_initialized || !sensorId.isValid()) {
        return false;
    }

    const int result = kv_set(
        key,
        sensorId.bytes,
        TemperatureSensorId::SIZE,
        0
    );

    return result == KV_SUCCESS;
}