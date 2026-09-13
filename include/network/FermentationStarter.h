#pragma once

#include <Arduino.h>
#include <ArduinoHttpClient.h>
#include <WiFiNINA.h>

class ServerClient;

// Delivers the single business-level start caused by IDLE -> RUNNING. HTTP
// retries are deliberately independent from the measurement WebSocket.
class FermentationStarter
{
public:
    explicit FermentationStarter(ServerClient& serverClient);

    void requestStart();
    void update();
    void resetSession();

private:
    bool postStart(const String& beerId);
    ServerClient& _serverClient;
    WiFiClient _wifiClient;
    bool _requested = false;
    bool _completed = false;
    unsigned long _nextAttemptMs = 0;
    unsigned long _retryIntervalMs = 5000;

    static constexpr unsigned long MAX_RETRY_INTERVAL_MS = 60000;
};
