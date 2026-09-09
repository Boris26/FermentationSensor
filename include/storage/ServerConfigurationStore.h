#pragma once

#include "network/ServerConfiguration.h"
#include "storage/FlashStorage.h"

class ServerConfigurationStore
{
public:
    explicit ServerConfigurationStore(
        FlashStorage& storage
    );

    bool begin();

    bool hasConfiguration() const;

    ServerConfiguration load() const;

    bool save(
        const ServerConfiguration& configuration
    );

    bool clear();

private:
    struct StoredServerConfiguration
    {
        char host[64];
        uint16_t port;
        char path[96];
    };

    static constexpr const char* STORAGE_KEY =
        "server_config";

    FlashStorage& _storage;

    bool _initialized = false;
};