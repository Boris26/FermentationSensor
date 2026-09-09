#include "storage/ServerConfigurationStore.h"

#include <Arduino.h>
#include <cstring>


ServerConfigurationStore::ServerConfigurationStore(
    FlashStorage& storage
)
    : _storage(storage)
{
}


bool ServerConfigurationStore::begin()
{
    _initialized = true;

    Serial.println(
        "ServerConfigurationStore: ready."
    );

    return true;
}


bool ServerConfigurationStore::hasConfiguration() const
{
    if (!_initialized) {
        return false;
    }

    return _storage.exists(
        STORAGE_KEY
    );
}


ServerConfiguration ServerConfigurationStore::load() const
{
    ServerConfiguration configuration;

    if (!_initialized) {
        return configuration;
    }


    StoredServerConfiguration stored = {};


    if (
        !_storage.getBytes(
            STORAGE_KEY,
            &stored,
            sizeof(stored)
        )
    ) {
        return configuration;
    }


    configuration.host =
        String(stored.host);

    configuration.port =
        stored.port;

    configuration.path =
        String(stored.path);


    if (!configuration.isValid()) {
        return ServerConfiguration{};
    }


    return configuration;
}


bool ServerConfigurationStore::save(
    const ServerConfiguration& configuration
)
{
    if (
        !_initialized ||
        !configuration.isValid()
    ) {
        return false;
    }


    StoredServerConfiguration stored = {};


    configuration.host.toCharArray(
        stored.host,
        sizeof(stored.host)
    );


    stored.port =
        configuration.port;


    configuration.path.toCharArray(
        stored.path,
        sizeof(stored.path)
    );


    const bool success =
        _storage.setBytes(
            STORAGE_KEY,
            &stored,
            sizeof(stored)
        );


    if (!success) {
        Serial.println(
            "ServerConfigurationStore: failed to save configuration."
        );

        return false;
    }


    Serial.println(
        "ServerConfigurationStore: configuration saved."
    );

    return true;
}


bool ServerConfigurationStore::clear()
{
    if (!_initialized) {
        return false;
    }


    if (
        !_storage.exists(
            STORAGE_KEY
        )
    ) {
        return true;
    }


    return _storage.remove(
        STORAGE_KEY
    );
}