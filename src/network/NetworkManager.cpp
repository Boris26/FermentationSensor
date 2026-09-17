#include "network/NetworkManager.h"

#include <WiFiNINA.h>

namespace
{
constexpr unsigned long CONNECTION_RETRY_INTERVAL_MS = 10000;
constexpr unsigned long WIFI_CONNECTION_TIMEOUT_MS = 1000;
constexpr unsigned long WIFI_RESTART_SETTLE_MS = 1000;

void printNetworkDiagnostics()
{
    Serial.print("NetworkManager: status="); Serial.print(WiFi.status());
    Serial.print(" ip="); Serial.print(WiFi.localIP());
    Serial.print(" gateway="); Serial.print(WiFi.gatewayIP());
    Serial.print(" dns="); Serial.print(WiFi.dnsIP());
    Serial.print(" rssi="); Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}
}

void NetworkManager::begin(const WifiCredentials& credentials)
{
    Serial.println("NetworkManager: initializing WiFi...");

    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("NetworkManager: WiFi module not found.");
        return;
    }

    // WiFi.begin() may block while trying to establish
    // the connection. Keep this timeout short so that
    // the main loop can continue running.
    WiFi.setTimeout(
        WIFI_CONNECTION_TIMEOUT_MS
    );

    _credentials = credentials;

    if (!_credentials.isValid()) {
        Serial.println("NetworkManager: no valid WiFi credentials.");
        return;
    }

    connect();
}

void NetworkManager::update()
{
    const int wifiStatus = WiFi.status();
    if (wifiStatus != _lastReportedStatus) {
        Serial.print('['); Serial.print(millis());
        Serial.print(" ms] WIFI_STATUS_CHANGED status="); Serial.print(wifiStatus);
        Serial.print(" rssi="); Serial.println(wifiStatus == WL_CONNECTED ? WiFi.RSSI() : 0);
        _lastReportedStatus = wifiStatus;
    }

    if (!_credentials.isValid()) {
        return;
    }

    const unsigned long now = millis();

    if (_restartPending) {
        if (now - _restartRequestedMs < WIFI_RESTART_SETTLE_MS) return;
        _restartPending = false;
        connect();
        return;
    }

    if (wifiStatus == WL_CONNECTED) {
        if (_connectionStarted) {
            _connectionStarted = false;

            Serial.println();
            Serial.print('['); Serial.print(now);
            Serial.println(" ms] WIFI_CONNECTED");

            printNetworkDiagnostics();
        }

        return;
    }

    if (!_connectionStarted) {
        connect();
        return;
    }

    if (
        now - _lastConnectionAttempt >=
        CONNECTION_RETRY_INTERVAL_MS
    ) {
        Serial.println(
            "NetworkManager: connection failed, retrying..."
        );

        connect();
    }
}

bool NetworkManager::isConnected() const
{
    return !_restartPending && WiFi.status() == WL_CONNECTED;
}

void NetworkManager::requestReconnect(const char* reason)
{
    if (!_credentials.isValid() || _restartPending) return;
    Serial.print("NetworkManager: controlled WiFi reset: ");
    Serial.println(reason);
    printNetworkDiagnostics();
    WiFi.disconnect();
    WiFi.end();
    _connectionStarted = false;
    _restartPending = true;
    _restartRequestedMs = millis();
}

void NetworkManager::connect()
{
    Serial.print('['); Serial.print(millis());
    Serial.print(" ms] WIFI_CONNECT_ATTEMPT ssid=");
    Serial.println(_credentials.ssid);

    _lastConnectionAttempt = millis();
    _connectionStarted = true;

    WiFi.begin(
        _credentials.ssid.c_str(),
        _credentials.password.c_str()
    );
}
