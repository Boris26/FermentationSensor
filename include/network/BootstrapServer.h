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

    bool consumeConfigurationChanged();

private:
    void begin();

    void handleClient(
        WiFiClient& client
    );

    void resetClient();

    bool parseConfiguration(
        const String& body,
        ServerConfiguration& configuration
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

    WiFiClient _client;

    String _requestLine;
    String _headerLine;
    String _body;

    int _contentLength = 0;

    unsigned long _clientStartedMs = 0;

    bool _started = false;
    bool _clientActive = false;
    bool _readingBody = false;
    bool _configurationChanged = false;

    static constexpr size_t READ_BUDGET_BYTES = 128;
    static constexpr unsigned long CLIENT_TIMEOUT_MS = 2000;
};
