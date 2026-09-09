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
    FlashStorage& _storage;

    bool _initialized = false;
};