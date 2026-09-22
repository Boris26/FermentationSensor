#include "network/NetworkManager.h"

#include <WiFi.h>
#include <cstring>

namespace
{
constexpr unsigned long CONNECTION_RETRY_INTERVAL_MS = 10000;
constexpr unsigned long WIFI_RESTART_SETTLE_MS = 1000;
constexpr unsigned long WIFI_RSSI_LOG_INTERVAL_MS = 30000;
constexpr unsigned long WIFI_ROAM_CHECK_INTERVAL_MS = 60000;
constexpr int32_t WIFI_ROAM_RSSI_THRESHOLD_DBM = -70;
constexpr int32_t WIFI_ROAM_MIN_IMPROVEMENT_DB = 10;

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

void configureAccessPointSelection()
{
    // The same SSID may be provided by multiple access points/repeaters.
    // Scan all channels before connecting and let the ESP32 select the
    // matching BSSID with the strongest RSSI instead of stopping at the
    // first acceptable access point it encounters.
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
}

void printBssid(const uint8_t* bssid)
{
    if (!bssid) {
        Serial.print("unknown");
        return;
    }

    for (uint8_t i = 0; i < 6; ++i) {
        if (bssid[i] < 16) Serial.print('0');
        Serial.print(bssid[i], HEX);
        if (i < 5) Serial.print(':');
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
    configureAccessPointSelection();
    if (!_credentials.isValid()) {
        Serial.println("NetworkManager: no valid WiFi credentials.");
        return;
    }
    Serial.println("NetworkManager: access point selection=all-channel strongest-signal");
    Serial.print("NetworkManager: roaming threshold=");
    Serial.print(WIFI_ROAM_RSSI_THRESHOLD_DBM);
    Serial.print(" dBm minimumImprovement=");
    Serial.print(WIFI_ROAM_MIN_IMPROVEMENT_DB);
    Serial.print(" dB checkIntervalMs=");
    Serial.println(WIFI_ROAM_CHECK_INTERVAL_MS);
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

    if (_roamScanActive && status != WL_CONNECTED) {
        cancelRoamingScan();
    }

    if (_restartPending) {
        if (now - _restartRequestedMs < WIFI_RESTART_SETTLE_MS) return;
        _restartPending = false;
        WiFi.mode(WIFI_STA);
        configureAccessPointSelection();
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

        updateRoaming(now);
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
    cancelRoamingScan();
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
    configureAccessPointSelection();
    Serial.print('['); Serial.print(now); Serial.print(" ms] WIFI_CONNECT_ATTEMPT ssid=");
    Serial.print(_credentials.ssid); Serial.print(" statusBefore=");
    Serial.println(wifiStatusName(WiFi.status()));
    _lastConnectionAttempt = now;
    _connectionStarted = true;
    WiFi.begin(_credentials.ssid.c_str(), _credentials.password.c_str());
}

void NetworkManager::connectToAccessPoint(
    int32_t channel,
    const uint8_t* bssid,
    int32_t targetRssi
)
{
    const unsigned long now = millis();
    configureAccessPointSelection();

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_ROAM_CONNECT bssid=");
    printBssid(bssid);
    Serial.print(" channel="); Serial.print(channel);
    Serial.print(" targetRssi="); Serial.print(targetRssi);
    Serial.println(" dBm");

    _lastConnectionAttempt = now;
    _connectionStarted = true;

    // A deliberate disconnect uses ASSOC_LEAVE, so auto-reconnect does not race
    // the targeted BSSID connection. Supplying channel + BSSID avoids another
    // broad AP selection round for this roaming handoff.
    WiFi.disconnect(false, false);
    WiFi.begin(
        _credentials.ssid.c_str(),
        _credentials.password.c_str(),
        channel,
        bssid,
        true
    );
}

void NetworkManager::updateRoaming(unsigned long now)
{
    if (_roamScanActive) {
        const int16_t scanResult = WiFi.scanComplete();
        if (scanResult == WIFI_SCAN_RUNNING) return;

        _roamScanActive = false;
        if (scanResult == WIFI_SCAN_FAILED) {
            Serial.print('['); Serial.print(now);
            Serial.println(" ms] WIFI_ROAM_SCAN_FAILED");
            WiFi.scanDelete();
            return;
        }

        finishRoamingScan(scanResult, now);
        return;
    }

    const int32_t currentRssi = WiFi.RSSI();
    if (currentRssi > WIFI_ROAM_RSSI_THRESHOLD_DBM) return;

    if (
        _lastRoamCheckMs != 0 &&
        now - _lastRoamCheckMs < WIFI_ROAM_CHECK_INTERVAL_MS
    ) {
        return;
    }

    startRoamingScan(now, currentRssi);
}

void NetworkManager::startRoamingScan(unsigned long now, int32_t currentRssi)
{
    _lastRoamCheckMs = now;

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_ROAM_SCAN_STARTED currentBssid=");
    Serial.print(WiFi.BSSIDstr());
    Serial.print(" currentRssi="); Serial.print(currentRssi);
    Serial.println(" dBm");

    // Async scan keeps the main loop cooperative while checking all APs.
    const int16_t scanResult = WiFi.scanNetworks(true);
    if (scanResult == WIFI_SCAN_RUNNING) {
        _roamScanActive = true;
        return;
    }

    if (scanResult == WIFI_SCAN_FAILED) {
        Serial.print('['); Serial.print(now);
        Serial.println(" ms] WIFI_ROAM_SCAN_FAILED_TO_START");
        WiFi.scanDelete();
        return;
    }

    // Normally async scanning reports WIFI_SCAN_RUNNING, but handle an
    // immediately available result defensively as well.
    finishRoamingScan(scanResult, now);
}

void NetworkManager::finishRoamingScan(int16_t networkCount, unsigned long now)
{
    uint8_t currentBssid[6] = {0};
    WiFi.BSSID(currentBssid);
    const int32_t currentRssi = WiFi.RSSI();

    bool foundAlternative = false;
    int32_t bestRssi = -127;
    int32_t bestChannel = 0;
    uint8_t bestBssid[6] = {0};

    for (int16_t i = 0; i < networkCount; ++i) {
        if (WiFi.SSID(i) != _credentials.ssid) continue;

        uint8_t* candidateBssid = WiFi.BSSID(i);
        if (!candidateBssid) continue;
        if (std::memcmp(candidateBssid, currentBssid, sizeof(currentBssid)) == 0) continue;

        const int32_t candidateRssi = WiFi.RSSI(i);
        if (!foundAlternative || candidateRssi > bestRssi) {
            foundAlternative = true;
            bestRssi = candidateRssi;
            bestChannel = WiFi.channel(i);
            std::memcpy(bestBssid, candidateBssid, sizeof(bestBssid));
        }
    }

    WiFi.scanDelete();

    if (!foundAlternative) {
        Serial.print('['); Serial.print(now);
        Serial.print(" ms] WIFI_ROAM_STAY reason=no_alternative currentRssi=");
        Serial.print(currentRssi); Serial.println(" dBm");
        return;
    }

    const int32_t improvement = bestRssi - currentRssi;
    if (improvement < WIFI_ROAM_MIN_IMPROVEMENT_DB) {
        Serial.print('['); Serial.print(now);
        Serial.print(" ms] WIFI_ROAM_STAY currentRssi=");
        Serial.print(currentRssi);
        Serial.print(" bestAlternativeRssi="); Serial.print(bestRssi);
        Serial.print(" improvement="); Serial.print(improvement);
        Serial.println(" dB");
        return;
    }

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_ROAM_SWITCH fromBssid=");
    printBssid(currentBssid);
    Serial.print(" fromRssi="); Serial.print(currentRssi);
    Serial.print(" toBssid="); printBssid(bestBssid);
    Serial.print(" toRssi="); Serial.print(bestRssi);
    Serial.print(" channel="); Serial.print(bestChannel);
    Serial.print(" improvement="); Serial.print(improvement);
    Serial.println(" dB");

    connectToAccessPoint(bestChannel, bestBssid, bestRssi);
}

void NetworkManager::cancelRoamingScan()
{
    if (!_roamScanActive) return;
    _roamScanActive = false;
    WiFi.scanDelete();
    Serial.println("NetworkManager: roaming scan cancelled.");
}

void NetworkManager::logConnectionFailure(int status, unsigned long now)
{
    Serial.print('['); Serial.print(now); Serial.print(" ms] WIFI_CONNECT_FAILED status=");
    Serial.print(status); Serial.print(" name="); Serial.print(wifiStatusName(status));
    Serial.print(" attemptAgeMs="); Serial.println(now - _lastConnectionAttempt);
}
