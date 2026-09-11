#pragma once

#include "network/GatewayEndpoint.h"
#include "storage/FlashStorage.h"

class GatewayEndpointStore
{
public:
    explicit GatewayEndpointStore(FlashStorage& storage);
    bool begin();
    GatewayEndpoint load() const;
    bool save(const GatewayEndpoint& endpoint);

private:
    static constexpr size_t MAX_ADDRESS_LENGTH = 15;
    static constexpr size_t MAX_PATH_LENGTH = 95;
    struct StoredEndpoint {
        char address[MAX_ADDRESS_LENGTH + 1] = {};
        uint16_t port = 0;
        char path[MAX_PATH_LENGTH + 1] = {};
        uint16_t protocolVersion = 0;
    };
    static constexpr const char* STORAGE_KEY = "gateway_cache";
    FlashStorage& _storage;
    bool _initialized = false;
};
