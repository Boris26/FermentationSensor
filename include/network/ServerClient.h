#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

#include "device/DeviceIdentity.h"
#include "network/GatewayEndpoint.h"
#include "network/MeasurementOutbox.h"


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

    const String& finishedBeerId() const;

    const GatewayEndpoint& endpoint() const;

    void onNetworkDisconnected();

    void stop();
    void requestReconnect();

    uint8_t failedConnectionCycles() const;

    bool sendMeasurement(const OutboxEntry& measurement, uint32_t nowMs);

    bool takeMeasurementAcknowledgement(uint32_t& sequence);


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

    bool parseMeasurementAcknowledgement(
        const String& message,
        uint32_t& sequence
    ) const;

    bool parseStringField(
        const String& message,
        const char* field,
        String& value
    ) const;


    DeviceIdentity& _deviceIdentity;

    WiFiClient _wifiClient;

    WebSocketClient* _webSocketClient =
        nullptr;

    GatewayEndpoint _endpoint;


    bool _configured = false;

    bool _connected = false;

    bool _registered = false;
    String _finishedBeerId;
    bool _hasMeasurementAcknowledgement = false;
    uint32_t _measurementAcknowledgementSequence = 0;

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
