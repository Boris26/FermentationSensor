#pragma once

#include "network/WifiCredentials.h"

class WifiCredentialStore
{
public:
    bool begin();

    bool hasCredentials() const;

    WifiCredentials load() const;

    bool save(const WifiCredentials& credentials);

    bool clear();

private:
    bool _initialized = false;
};