#pragma once

#include <Arduino.h>

#include "network/WifiCredentials.h"

class NetworkManager
{
public:
    void begin(const WifiCredentials& credentials);
    void update();

    bool isConnected() const;
    void requestReconnect(const char* reason);

private:
    void connect();

    WifiCredentials _credentials;

    unsigned long _lastConnectionAttempt = 0;
    bool _connectionStarted = false;
    bool _restartPending = false;
    unsigned long _restartRequestedMs = 0;
    int _lastReportedStatus = -1;
};
