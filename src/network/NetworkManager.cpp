#include "network/NetworkManager.h"

#include <WiFiNINA.h>

namespace
{
constexpr unsigned long CONNECTION_RETRY_INTERVAL_MS = 10000;
constexpr unsigned long WIFI_CONNECTION_TIMEOUT_MS = 1000;
constexpr unsigned long WIFI_RESTART_SETTLE_MS = 1000;
constexpr unsigned long WIFI_RSSI_LOG_INTERVAL_MS = 30000;
constexpr unsigned long WIFI_SCAN_DIAGNOSTIC_INTERVAL_MS = 60000;

const char* wifiStatusName(int status)
{
    // WiFiNINA uses the standard Arduino WiFi status values. Keep the
    // diagnostics independent of optional enum symbols so this also builds
    // with older WiFiNINA releases.
    switch (status) {
        case 0: return "IDLE";
        case 1: return "NO_SSID_AVAIL";
        case 2: return "SCAN_COMPLETED";
        case 3: return "CONNECTED";
        case 4: return "CONNECT_FAILED";
        case 5: return "CONNECTION_LOST";
        case 6: return "DISCONNECTED";
        case 7: return "AP_LISTENING";
        case 8: return "AP_CONNECTED";
        case 9: return "AP_FAILED";
        case 255: return "NO_MODULE";
        default: return "UNKNOWN";
    }
}

void printBssid(const uint8_t* bssid)
{
    for (int i = 0; i < 6; ++i) {
        if (i > 0) Serial.print(':');
        if (bssid[i] < 0x10) Serial.print('0');
        Serial.print(bssid[i], HEX);
    }
}

void printNetworkDiagnostics()
{
    const int status = WiFi.status();
    Serial.print("NetworkManager: status="); Serial.print(status);
    Serial.print('('); Serial.print(wifiStatusName(status)); Serial.print(')');
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
        Serial.print(" name="); Serial.print(wifiStatusName(wifiStatus));
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
            _lastRssiLogMs = now;
        } else if (now - _lastRssiLogMs >= WIFI_RSSI_LOG_INTERVAL_MS) {
            Serial.print('['); Serial.print(now);
            Serial.print(" ms] WIFI_RSSI rssi="); Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
            _lastRssiLogMs = now;
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
        logConnectionFailure(wifiStatus, now);

        if (
            _lastScanDiagnosticsMs == 0 ||
            now - _lastScanDiagnosticsMs >= WIFI_SCAN_DIAGNOSTIC_INTERVAL_MS
        ) {
            logTargetNetworkScan(now);
        }

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
    const unsigned long now = millis();
    const int statusBefore = WiFi.status();

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_CONNECT_ATTEMPT ssid=");
    Serial.print(_credentials.ssid);
    Serial.print(" statusBefore="); Serial.print(statusBefore);
    Serial.print(" name=");
    Serial.println(wifiStatusName(statusBefore));

    _lastConnectionAttempt = now;
    _connectionStarted = true;

    const int beginResult = WiFi.begin(
        _credentials.ssid.c_str(),
        _credentials.password.c_str()
    );

    Serial.print('['); Serial.print(millis());
    Serial.print(" ms] WIFI_BEGIN_RESULT status="); Serial.print(beginResult);
    Serial.print(" name=");
    Serial.println(wifiStatusName(beginResult));
}

void NetworkManager::logConnectionFailure(int wifiStatus, unsigned long now)
{
    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_CONNECT_FAILED status="); Serial.print(wifiStatus);
    Serial.print(" name="); Serial.print(wifiStatusName(wifiStatus));
    Serial.print(" attemptAgeMs=");
    Serial.println(now - _lastConnectionAttempt);
}

void NetworkManager::logTargetNetworkScan(unsigned long now)
{
    _lastScanDiagnosticsMs = now;

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_SCAN_START target=");
    Serial.println(_credentials.ssid);

    const int networkCount = WiFi.scanNetworks();

    Serial.print('['); Serial.print(millis());
    Serial.print(" ms] WIFI_SCAN_RESULT count=");
    Serial.println(networkCount);

    if (networkCount < 0) {
        Serial.println("WIFI_SCAN_FAILED");
        return;
    }

    int targetMatches = 0;
    for (int i = 0; i < networkCount; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid != _credentials.ssid) continue;

        ++targetMatches;
        uint8_t bssid[6] = {0};
        WiFi.BSSID(i, bssid);

        Serial.print("WIFI_SCAN_MATCH ssid="); Serial.print(ssid);
        Serial.print(" bssid="); printBssid(bssid);
        Serial.print(" rssi="); Serial.print(WiFi.RSSI(i));
        Serial.print(" dBm channel="); Serial.print(WiFi.channel(i));
        Serial.print(" encryptionType=");
        Serial.println(WiFi.encryptionType(i));
    }

    if (targetMatches == 0) {
        Serial.print("WIFI_SCAN_NO_MATCH target=");
        Serial.println(_credentials.ssid);
    }
}
