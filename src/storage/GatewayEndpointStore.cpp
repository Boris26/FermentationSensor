#include "storage/GatewayEndpointStore.h"

#include <cstring>

GatewayEndpointStore::GatewayEndpointStore(FlashStorage& storage) : _storage(storage) {}

bool GatewayEndpointStore::begin()
{
    _initialized = true;
    Serial.println("GatewayEndpointStore: cache ready.");
    return true;
}

GatewayEndpoint GatewayEndpointStore::load() const
{
    StoredEndpoint stored = {};
    if (!_initialized || !_storage.exists(STORAGE_KEY) ||
        !_storage.getBytes(STORAGE_KEY, &stored, sizeof(stored)) ||
        stored.address[MAX_ADDRESS_LENGTH] != '\0' ||
        stored.path[MAX_PATH_LENGTH] != '\0') {
        return {};
    }

    GatewayEndpoint endpoint;
    endpoint.address = stored.address;
    endpoint.port = stored.port;
    endpoint.path = stored.path;
    endpoint.protocolVersion = stored.protocolVersion;
    return endpoint.isValid() ? endpoint : GatewayEndpoint{};
}

bool GatewayEndpointStore::save(const GatewayEndpoint& endpoint)
{
    if (!_initialized || !endpoint.isValid() ||
        endpoint.address.length() > MAX_ADDRESS_LENGTH ||
        endpoint.path.length() > MAX_PATH_LENGTH) {
        return false;
    }

    StoredEndpoint stored = {};
    endpoint.address.toCharArray(stored.address, sizeof(stored.address));
    stored.port = endpoint.port;
    endpoint.path.toCharArray(stored.path, sizeof(stored.path));
    stored.protocolVersion = endpoint.protocolVersion;
    const bool saved = _storage.setBytes(STORAGE_KEY, &stored, sizeof(stored));
    Serial.println(saved ? "GatewayEndpointStore: cache updated."
                         : "GatewayEndpointStore: cache update failed.");
    return saved;
}
