#include "network/NetworkManager.h"

#include <WiFiNINA.h>

namespace
{
constexpr unsigned long CONNECTION_RETRY_INTERVAL_MS = 10000;
}

void NetworkManager::begin(const WifiCredentials& credentials)
{
    Serial.println("NetworkManager: initializing WiFi...");

    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("NetworkManager: WiFi module not found.");
        return;
    }

    _credentials = credentials;

    if (!_credentials.isValid()) {
        Serial.println("NetworkManager: no valid WiFi credentials.");
        return;
    }

    connect();
}

void NetworkManager::update()
{
    if (!_credentials.isValid()) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (_connectionStarted) {
            _connectionStarted = false;

            Serial.println();
            Serial.println("NetworkManager: connected.");

            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());

            Serial.print("Signal strength: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        }

        return;
    }

    const unsigned long now = millis();

    if (!_connectionStarted) {
        connect();
        return;
    }

    if (now - _lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL_MS) {
        Serial.println(
            "NetworkManager: connection failed, retrying..."
        );

        connect();
    }
}

bool NetworkManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::connect()
{
    Serial.print("NetworkManager: connecting to ");
    Serial.println(_credentials.ssid);

    _lastConnectionAttempt = millis();
    _connectionStarted = true;

    WiFi.begin(
        _credentials.ssid.c_str(),
        _credentials.password.c_str()
    );
}