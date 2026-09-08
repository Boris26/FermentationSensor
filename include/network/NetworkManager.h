#pragma once

#include <Arduino.h>

#include "network/WifiCredentials.h"

class NetworkManager
{
public:
    void begin(const WifiCredentials& credentials);
    void update();

    bool isConnected() const;

private:
    void connect();

    WifiCredentials _credentials;

    unsigned long _lastConnectionAttempt = 0;
    bool _connectionStarted = false;
};