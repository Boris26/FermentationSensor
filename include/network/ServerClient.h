#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

#include "device/DeviceIdentity.h"
#include "network/ServerConfiguration.h"


class ServerClient
{
public:
    explicit ServerClient(
        DeviceIdentity& deviceIdentity
    );

    void begin(
        const ServerConfiguration& configuration
    );

    void update();

    bool isConnected() const;


private:
    void connect();

    void disconnect();

    void sendRegistration();

    void handleIncomingMessages();


    DeviceIdentity& _deviceIdentity;

    WiFiClient _wifiClient;

    WebSocketClient* _webSocketClient =
        nullptr;

    ServerConfiguration _configuration;


    bool _configured = false;

    bool _connected = false;


    unsigned long _lastConnectionAttemptMs = 0;


    static constexpr unsigned long
        RECONNECT_INTERVAL_MS = 5000;
};