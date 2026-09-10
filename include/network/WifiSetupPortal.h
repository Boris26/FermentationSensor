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

    String getQueryParameter(
        const String& request,
        const String& name
    );

    String urlDecode(const String& value);

    WiFiServer _server;
    WiFiClient _client;
    WifiCredentialStore& _credentialStore;

    bool _active = false;
    bool _clientActive = false;

    String _requestLine;
    unsigned long _clientStartedMs = 0;

    static constexpr size_t READ_BUDGET_BYTES = 128;
    static constexpr unsigned long CLIENT_TIMEOUT_MS = 1000;
};
