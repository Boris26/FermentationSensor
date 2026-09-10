#pragma once

#include "network/WifiCredentials.h"
#include "storage/FlashStorage.h"

class WifiCredentialStore
{
public:
    explicit WifiCredentialStore(
        FlashStorage& storage
    );

    bool begin();

    bool hasCredentials() const;

    WifiCredentials load() const;

    bool save(
        const WifiCredentials& credentials
    );

    bool clear();

private:
    static constexpr uint8_t STORAGE_VERSION = 1;
    static constexpr size_t MAX_SSID_LENGTH = 32;
    static constexpr size_t MAX_PASSWORD_LENGTH = 63;

    struct StoredWifiConfiguration
    {
        uint8_t version = STORAGE_VERSION;
        char ssid[MAX_SSID_LENGTH + 1] = {};
        char password[MAX_PASSWORD_LENGTH + 1] = {};
    };

    bool isValid(
        const WifiCredentials& credentials
    ) const;

    bool removeIfPresent(
        const char* key
    );

    FlashStorage& _storage;

    bool _initialized = false;
};
