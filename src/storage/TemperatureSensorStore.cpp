#include "storage/TemperatureSensorStore.h"

#include <Arduino.h>
#include <cstring>

namespace
{
constexpr char TEMPERATURE_CONFIG_KEY[] =
    "temperature_config";
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
    StoredTemperatureConfiguration configuration;

    if (!loadConfiguration(configuration)) {
        return false;
    }

    return configuration.hasAmbient;
}


bool TemperatureSensorStore::hasBeerSensor() const
{
    StoredTemperatureConfiguration configuration;

    if (!loadConfiguration(configuration)) {
        return false;
    }

    return configuration.hasBeer;
}


TemperatureSensorId TemperatureSensorStore::loadAmbientSensor() const
{
    StoredTemperatureConfiguration configuration;

    if (
        !loadConfiguration(configuration) ||
        !configuration.hasAmbient
    ) {
        return TemperatureSensorId{};
    }

    return toSensorId(
        configuration.ambient
    );
}


TemperatureSensorId TemperatureSensorStore::loadBeerSensor() const
{
    StoredTemperatureConfiguration configuration;

    if (
        !loadConfiguration(configuration) ||
        !configuration.hasBeer
    ) {
        return TemperatureSensorId{};
    }

    return toSensorId(
        configuration.beer
    );
}


bool TemperatureSensorStore::saveAmbientSensor(
    const TemperatureSensorId& sensorId
)
{
    if (
        !_initialized ||
        !sensorId.isValid()
    ) {
        return false;
    }

    StoredTemperatureConfiguration configuration;

    loadConfiguration(
        configuration
    );

    memcpy(
        configuration.ambient,
        sensorId.bytes,
        TemperatureSensorId::SIZE
    );

    configuration.hasAmbient = true;

    return saveConfiguration(
        configuration
    );
}


bool TemperatureSensorStore::saveBeerSensor(
    const TemperatureSensorId& sensorId
)
{
    if (
        !_initialized ||
        !sensorId.isValid()
    ) {
        return false;
    }

    StoredTemperatureConfiguration configuration;

    loadConfiguration(
        configuration
    );

    memcpy(
        configuration.beer,
        sensorId.bytes,
        TemperatureSensorId::SIZE
    );

    configuration.hasBeer = true;

    return saveConfiguration(
        configuration
    );
}


bool TemperatureSensorStore::clearAmbientSensor()
{
    if (!_initialized) {
        return false;
    }

    StoredTemperatureConfiguration configuration;

    if (!loadConfiguration(configuration)) {
        return true;
    }

    configuration.hasAmbient = false;

    memset(
        configuration.ambient,
        0,
        sizeof(configuration.ambient)
    );

    return saveConfiguration(
        configuration
    );
}


bool TemperatureSensorStore::clearBeerSensor()
{
    if (!_initialized) {
        return false;
    }

    StoredTemperatureConfiguration configuration;

    if (!loadConfiguration(configuration)) {
        return true;
    }

    configuration.hasBeer = false;

    memset(
        configuration.beer,
        0,
        sizeof(configuration.beer)
    );

    return saveConfiguration(
        configuration
    );
}


bool TemperatureSensorStore::loadConfiguration(
    StoredTemperatureConfiguration& configuration
) const
{
    if (!_initialized) {
        return false;
    }

    if (
        !_storage.exists(
            TEMPERATURE_CONFIG_KEY
        )
    ) {
        configuration =
            StoredTemperatureConfiguration{};

        return true;
    }

    return _storage.getBytes(
        TEMPERATURE_CONFIG_KEY,
        &configuration,
        sizeof(configuration)
    );
}


bool TemperatureSensorStore::saveConfiguration(
    const StoredTemperatureConfiguration& configuration
)
{
    if (!_initialized) {
        return false;
    }

    return _storage.setBytes(
        TEMPERATURE_CONFIG_KEY,
        &configuration,
        sizeof(configuration)
    );
}


TemperatureSensorId TemperatureSensorStore::toSensorId(
    const uint8_t* bytes
) const
{
    TemperatureSensorId sensorId;

    memcpy(
        sensorId.bytes,
        bytes,
        TemperatureSensorId::SIZE
    );

    return sensorId;
}