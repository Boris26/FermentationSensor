#include "network/NetworkManager.h"

#include <WiFi.h>

namespace
{
constexpr unsigned long CONNECTION_RETRY_INTERVAL_MS = 10000;
constexpr unsigned long WIFI_RESTART_SETTLE_MS = 1000;
constexpr unsigned long WIFI_RSSI_LOG_INTERVAL_MS = 30000;

const char* wifiStatusName(int status)
{
    switch (status) {
        case WL_IDLE_STATUS: return "IDLE";
        case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

void printNetworkDiagnostics()
{
    Serial.print("NetworkManager: status="); Serial.print(WiFi.status());
    Serial.print(" ssid="); Serial.print(WiFi.SSID());
    Serial.print(" bssid="); Serial.print(WiFi.BSSIDstr());
    Serial.print(" channel="); Serial.print(WiFi.channel());
    Serial.print(" ip="); Serial.print(WiFi.localIP());
    Serial.print(" gateway="); Serial.print(WiFi.gatewayIP());
    Serial.print(" dns="); Serial.print(WiFi.dnsIP());
    Serial.print(" rssi="); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
}
}

void NetworkManager::begin(const WifiCredentials& credentials)
{
    _credentials = credentials;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    if (!_credentials.isValid()) {
        Serial.println("NetworkManager: no valid WiFi credentials.");
        return;
    }
    connect();
}

void NetworkManager::update()
{
    const int status = WiFi.status();
    const unsigned long now = millis();
    if (status != _lastReportedStatus) {
        Serial.print('['); Serial.print(now); Serial.print(" ms] WIFI_STATUS_CHANGED status=");
        Serial.print(status); Serial.print(" name="); Serial.println(wifiStatusName(status));
        _lastReportedStatus = status;
    }
    if (!_credentials.isValid()) return;
    if (_restartPending) {
        if (now - _restartRequestedMs < WIFI_RESTART_SETTLE_MS) return;
        _restartPending = false;
        WiFi.mode(WIFI_STA);
        connect();
        return;
    }
    if (status == WL_CONNECTED) {
        if (_connectionStarted) {
            _connectionStarted = false;
            Serial.print('['); Serial.print(now); Serial.println(" ms] WIFI_CONNECTED");
            printNetworkDiagnostics();
            _lastRssiLogMs = now;
        } else if (now - _lastRssiLogMs >= WIFI_RSSI_LOG_INTERVAL_MS) {
            Serial.print('['); Serial.print(now); Serial.print(" ms] WIFI_RSSI rssi=");
            Serial.print(WiFi.RSSI()); Serial.println(" dBm");
            _lastRssiLogMs = now;
        }
        return;
    }
    if (!_connectionStarted || now - _lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL_MS) {
        if (_connectionStarted) logConnectionFailure(status, now);
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
    Serial.print("NetworkManager: controlled WiFi reset: "); Serial.println(reason);
    printNetworkDiagnostics();
    WiFi.disconnect(true, false);
    _connectionStarted = false;
    _restartPending = true;
    _restartRequestedMs = millis();
}

void NetworkManager::connect()
{
    const unsigned long now = millis();
    Serial.print('['); Serial.print(now); Serial.print(" ms] WIFI_CONNECT_ATTEMPT ssid=");
    Serial.print(_credentials.ssid); Serial.print(" statusBefore=");
    Serial.println(wifiStatusName(WiFi.status()));
    _lastConnectionAttempt = now;
    _connectionStarted = true;
    WiFi.begin(_credentials.ssid.c_str(), _credentials.password.c_str());
}

void NetworkManager::logConnectionFailure(int status, unsigned long now)
{
    Serial.print('['); Serial.print(now); Serial.print(" ms] WIFI_CONNECT_FAILED status=");
    Serial.print(status); Serial.print(" name="); Serial.print(wifiStatusName(status));
    Serial.print(" attemptAgeMs="); Serial.println(now - _lastConnectionAttempt);
}
