#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>

#include "network/WifiCredentialStore.h"

class WifiSetupPortal
{
public:
    explicit WifiSetupPortal(WifiCredentialStore& credentialStore);

    void begin();
    void update();

    bool isActive() const;

private:
    void handleClient(WiFiClient& client);
    void sendSetupPage(WiFiClient& client);
    void sendSuccessPage(WiFiClient& client);

    String getQueryParameter(
        const String& request,
        const String& name
    );

    String urlDecode(const String& value);

    WiFiServer _server;
    WifiCredentialStore& _credentialStore;

    bool _active = false;
};