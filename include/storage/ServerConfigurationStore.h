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
    static constexpr size_t MAX_HOST_LENGTH = 63;
    static constexpr size_t MAX_PATH_LENGTH = 95;

    struct StoredServerConfiguration
    {
        char host[MAX_HOST_LENGTH + 1] = {};
        uint16_t port = 0;
        char path[MAX_PATH_LENGTH + 1] = {};
    };

    static constexpr const char* STORAGE_KEY =
        "server_config";

    FlashStorage& _storage;

    bool _initialized = false;
};
