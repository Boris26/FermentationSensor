#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

#include "device/DeviceIdentity.h"
#include "network/GatewayEndpoint.h"


class ServerClient
{
public:
    explicit ServerClient(
        DeviceIdentity& deviceIdentity
    );

    void begin(
        const GatewayEndpoint& endpoint
    );

    void update();

    bool isConnected() const;

    bool isRegistered() const;

    void onNetworkDisconnected();

    void stop();

    uint8_t failedConnectionCycles() const;

    bool sendTemperatureMeasurement(
        float beerTemperature,
        float ambientTemperature,
        bool pressureAvailable,
        float pressurePa
    );


private:
    void connect();

    void disconnect();

    void sendRegistration();

    bool sendTextMessage(
        const String& message,
        const char* description
    );

    void handleIncomingMessages();

    void handleMessage(
        const String& message
    );

    bool parseMessageType(
        const String& message,
        String& type
    ) const;


    DeviceIdentity& _deviceIdentity;

    WiFiClient _wifiClient;

    WebSocketClient* _webSocketClient =
        nullptr;

    GatewayEndpoint _endpoint;


    bool _configured = false;

    bool _connected = false;

    bool _registered = false;

    uint8_t _failedConnectionCycles = 0;


    unsigned long _lastConnectionAttemptMs = 0;

    unsigned long _registrationSentMs = 0;

    unsigned long _reconnectIntervalMs =
        INITIAL_RECONNECT_INTERVAL_MS;


    static constexpr unsigned long
        INITIAL_RECONNECT_INTERVAL_MS = 5000;

    static constexpr unsigned long
        MAX_RECONNECT_INTERVAL_MS = 60000;

    static constexpr unsigned long
        REGISTRATION_TIMEOUT_MS = 7500;

    static constexpr size_t
        MAX_WEBSOCKET_MESSAGE_SIZE = WS_TX_BUFFER_SIZE;
};
