#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>

#include "storage/ServerConfigurationStore.h"

class BootstrapServer
{
public:
    explicit BootstrapServer(
        ServerConfigurationStore& configurationStore
    );

    void update();

private:
    void begin();

    void handleClient(
        WiFiClient& client
    );

    bool parseConfiguration(
        const String& body,
        ServerConfiguration& configuration
    );

    bool readJsonString(
        const String& json,
        const char* key,
        String& value
    );

    bool readJsonNumber(
        const String& json,
        const char* key,
        uint16_t& value
    );

    void sendResponse(
        WiFiClient& client,
        int statusCode,
        const char* statusText,
        const char* body
    );

    ServerConfigurationStore&
        _configurationStore;

    WiFiServer _server{80};

    bool _started = false;
};