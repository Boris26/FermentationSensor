#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>

#include "storage/WifiCredentialStore.h"

class WifiSetupPortal
{
public:
    explicit WifiSetupPortal(WifiCredentialStore& credentialStore);

    void begin();
    void update();
    bool isActive() const;

private:
    void handleClient(WiFiClient& client);
    void resetClient();
    void sendSetupPage(WiFiClient& client);
    void sendSuccessPage(WiFiClient& client);
    void sendBadRequest(WiFiClient& client);

    bool getFormParameter(
        const String& body,
        const String& name,
        String& value
    ) const;
    bool urlDecode(const String& value, String& decoded) const;

    WiFiServer _server;
    WiFiClient _client;
    WifiCredentialStore& _credentialStore;

    bool _active = false;
    bool _clientActive = false;
    bool _readingBody = false;
    bool _formContentType = false;

    String _requestLine;
    String _headerLine;
    String _body;
    int _contentLength = 0;
    unsigned long _clientStartedMs = 0;

    static constexpr size_t READ_BUDGET_BYTES = 128;
    static constexpr unsigned long CLIENT_TIMEOUT_MS = 1000;
};
