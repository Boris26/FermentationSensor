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
    return load().isValid();
}


ServerConfiguration ServerConfigurationStore::load() const
{
    ServerConfiguration configuration;

    if (!_initialized) {
        return configuration;
    }


    StoredServerConfiguration stored = {};


    if (!_storage.exists(STORAGE_KEY)) {
        return configuration;
    }


    if (
        !_storage.getBytes(
            STORAGE_KEY,
            &stored,
            sizeof(stored)
        )
    ) {
        Serial.println(
            "ServerConfigurationStore: failed to load configuration."
        );

        return configuration;
    }


    if (
        stored.host[MAX_HOST_LENGTH] != '\0' ||
        stored.path[MAX_PATH_LENGTH] != '\0'
    ) {
        Serial.println(
            "ServerConfigurationStore: stored configuration is invalid."
        );

        return configuration;
    }


    configuration.host =
        String(stored.host);

    configuration.port =
        stored.port;

    configuration.path =
        String(stored.path);


    if (!configuration.isValid()) {
        Serial.println(
            "ServerConfigurationStore: stored configuration is incomplete."
        );

        return ServerConfiguration{};
    }


    return configuration;
}


bool ServerConfigurationStore::save(
    const ServerConfiguration& configuration
)
{
    if (!_initialized) {
        return false;
    }


    if (!configuration.isValid()) {
        Serial.println(
            "ServerConfigurationStore: invalid configuration."
        );

        return false;
    }

    if (
        configuration.host.length() > MAX_HOST_LENGTH ||
        configuration.path.length() > MAX_PATH_LENGTH
    ) {
        Serial.println(
            "ServerConfigurationStore: host or path is too long."
        );

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
