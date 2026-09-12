#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>
#include "config/SensorConfigService.h"
#include "device/DeviceIdentity.h"
#include "session/MeasurementSession.h"

class ServerClient;

class ConfigHttpServer {
public:
    ConfigHttpServer(SensorConfigService& config, DeviceIdentity& identity,
        MeasurementSession& session, ServerClient& gateway);
    void begin();
    void update();
private:
    void resetClient();
    void processRequest();
    void respond(int status, const char* contentType, const String& body);
    SensorConfigService& _config;
    DeviceIdentity& _identity;
    MeasurementSession& _session;
    ServerClient& _gateway;
    WiFiServer _server;
    WiFiClient _client;
    String _headers;
    String _body;
    size_t _contentLength = 0;
    bool _headersComplete = false;
    bool _started = false;
    unsigned long _lastActivityMs = 0;
};
