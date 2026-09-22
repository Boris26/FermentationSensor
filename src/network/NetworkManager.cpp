#include "network/NetworkManager.h"

#include <WiFi.h>
#include <cstring>

namespace
{
constexpr unsigned long CONNECTION_RETRY_INTERVAL_MS = 10000;
constexpr unsigned long WIFI_RESTART_SETTLE_MS = 1000;
constexpr unsigned long WIFI_RSSI_LOG_INTERVAL_MS = 30000;
constexpr unsigned long WIFI_ROAM_CHECK_INTERVAL_MS = 60000;
constexpr unsigned long WIFI_AP_SCAN_SETTLE_MS = 500;
constexpr unsigned long WIFI_AP_SCAN_RETRY_DELAY_MS = 1500;
constexpr uint8_t WIFI_AP_SCAN_MAX_FAILURES = 2;
constexpr int32_t WIFI_ROAM_RSSI_THRESHOLD_DBM = -70;

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

void configureFallbackAccessPointSelection()
{
    // Fallback only. Normal operation explicitly scans first and then connects
    // to the selected BSSID + channel.
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
    WiFi.setAutoReconnect(false);
    WiFi.persistent(false);
    configureFallbackAccessPointSelection();

    if (!_credentials.isValid()) {
        Serial.println("NetworkManager: no valid WiFi credentials.");
        return;
    }

    Serial.println("NetworkManager: access point selection=pre-connect scan + targeted BSSID");
    Serial.println("NetworkManager: fallback access point selection=all-channel strongest-signal");
    Serial.print("NetworkManager: roaming threshold=");
    Serial.print(WIFI_ROAM_RSSI_THRESHOLD_DBM);
    Serial.print(" dBm scanSettleMs=");
    Serial.print(WIFI_AP_SCAN_SETTLE_MS);
    Serial.print(" retryDelayMs=");
    Serial.print(WIFI_AP_SCAN_RETRY_DELAY_MS);
    Serial.print(" checkIntervalMs=");
    Serial.println(WIFI_ROAM_CHECK_INTERVAL_MS);

    scheduleAccessPointSelection(millis(), "initial", false);
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
        WiFi.setAutoReconnect(false);
        configureFallbackAccessPointSelection();
        scheduleAccessPointSelection(now, "controlled_reset", false);
        return;
    }

    if (_apSelectionActive) {
        updateAccessPointSelection(now);
        return;
    }

    if (status == WL_CONNECTED) {
        if (_connectionStarted) {
            _connectionStarted = false;
            Serial.print('['); Serial.print(now); Serial.println(" ms] WIFI_CONNECTED");
            printNetworkDiagnostics();
            _lastRssiLogMs = now;
            _lastRoamCheckMs = now;
        } else if (now - _lastRssiLogMs >= WIFI_RSSI_LOG_INTERVAL_MS) {
            Serial.print('['); Serial.print(now); Serial.print(" ms] WIFI_RSSI rssi=");
            Serial.print(WiFi.RSSI()); Serial.println(" dBm");
            _lastRssiLogMs = now;
        }

        const int32_t currentRssi = WiFi.RSSI();
        if (
            currentRssi <= WIFI_ROAM_RSSI_THRESHOLD_DBM &&
            now - _lastRoamCheckMs >= WIFI_ROAM_CHECK_INTERVAL_MS
        ) {
            _lastRoamCheckMs = now;
            scheduleAccessPointSelection(now, "weak_signal", true);
        }
        return;
    }

    if (_connectionStarted) {
        if (now - _lastConnectionAttempt < CONNECTION_RETRY_INTERVAL_MS) return;
        logConnectionFailure(status, now);
        _connectionStarted = false;
    }

    if (
        _lastConnectionAttempt == 0 ||
        now - _lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL_MS
    ) {
        scheduleAccessPointSelection(now, "reconnect", false);
    }
}

bool NetworkManager::isConnected() const
{
    return !_restartPending && WiFi.status() == WL_CONNECTED;
}

void NetworkManager::requestReconnect(const char* reason)
{
    if (!_credentials.isValid() || _restartPending) return;

    cancelAccessPointSelection();
    Serial.print("NetworkManager: controlled WiFi reset: "); Serial.println(reason);
    printNetworkDiagnostics();

    WiFi.disconnect(true, false);
    _connectionStarted = false;
    _restartPending = true;
    _restartRequestedMs = millis();
}

void NetworkManager::scheduleAccessPointSelection(
    unsigned long now,
    const char* reason,
    bool disconnectCurrent
)
{
    if (_apSelectionActive) return;

    _apSelectionActive = true;
    _apScanSettlePending = true;
    _apScanSettleStartedMs = now;
    _apScanActive = false;
    _apScanRetryPending = false;
    _apScanRetryRequestedMs = 0;
    _apScanFailureCount = 0;
    _apSelectionReason = reason == nullptr ? "unknown" : reason;

    WiFi.scanDelete();

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_AP_SELECTION_STARTED reason=");
    Serial.print(_apSelectionReason);
    Serial.print(" ssid="); Serial.println(_credentials.ssid);

    if (disconnectCurrent && WiFi.status() == WL_CONNECTED) {
        Serial.print('['); Serial.print(now);
        Serial.print(" ms] WIFI_AP_SELECTION_DISCONNECT bssid=");
        Serial.print(WiFi.BSSIDstr());
        Serial.print(" rssi="); Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        WiFi.disconnect(false, false);
        _connectionStarted = false;
    }

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_AP_SCAN_WAIT settleMs=");
    Serial.println(WIFI_AP_SCAN_SETTLE_MS);
}

void NetworkManager::updateAccessPointSelection(unsigned long now)
{
    if (_apScanSettlePending) {
        if (now - _apScanSettleStartedMs < WIFI_AP_SCAN_SETTLE_MS) return;
        _apScanSettlePending = false;
        startAccessPointScan(now);
        return;
    }

    if (_apScanActive) {
        const int16_t scanResult = WiFi.scanComplete();
        if (scanResult == WIFI_SCAN_RUNNING) return;

        _apScanActive = false;
        if (scanResult == WIFI_SCAN_FAILED) {
            handleAccessPointScanFailure(now, "WIFI_AP_SCAN_FAILED");
            return;
        }

        finishAccessPointScan(scanResult, now);
        return;
    }

    if (_apScanRetryPending) {
        if (now - _apScanRetryRequestedMs < WIFI_AP_SCAN_RETRY_DELAY_MS) return;
        _apScanRetryPending = false;
        startAccessPointScan(now);
    }
}

void NetworkManager::startAccessPointScan(unsigned long now)
{
    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_AP_SCAN_STARTED reason=");
    Serial.print(_apSelectionReason);
    Serial.print(" attempt=");
    Serial.println(_apScanFailureCount + 1);

    // The scan deliberately runs while not associated with an AP. Earlier field
    // tests showed that scans started on an active weak connection repeatedly
    // timed out after about six seconds on this Arduino-ESP32 version.
    const int16_t scanResult = WiFi.scanNetworks(true);
    if (scanResult == WIFI_SCAN_RUNNING) {
        _apScanActive = true;
        return;
    }

    if (scanResult == WIFI_SCAN_FAILED) {
        handleAccessPointScanFailure(now, "WIFI_AP_SCAN_FAILED_TO_START");
        return;
    }

    finishAccessPointScan(scanResult, now);
}

void NetworkManager::finishAccessPointScan(
    int16_t networkCount,
    unsigned long now
)
{
    bool foundTarget = false;
    int32_t bestRssi = -127;
    int32_t bestChannel = 0;
    uint8_t bestBssid[6] = {0};
    uint16_t targetCount = 0;

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_AP_SCAN_COMPLETE networks=");
    Serial.println(networkCount);

    for (int16_t i = 0; i < networkCount; ++i) {
        if (WiFi.SSID(i) != _credentials.ssid) continue;

        uint8_t* candidateBssid = WiFi.BSSID(i);
        if (!candidateBssid) continue;

        ++targetCount;
        const int32_t candidateRssi = WiFi.RSSI(i);
        const int32_t candidateChannel = WiFi.channel(i);

        Serial.print('['); Serial.print(now);
        Serial.print(" ms] WIFI_AP_CANDIDATE ssid=");
        Serial.print(_credentials.ssid);
        Serial.print(" bssid="); printBssid(candidateBssid);
        Serial.print(" channel="); Serial.print(candidateChannel);
        Serial.print(" rssi="); Serial.print(candidateRssi);
        Serial.println(" dBm");

        if (!foundTarget || candidateRssi > bestRssi) {
            foundTarget = true;
            bestRssi = candidateRssi;
            bestChannel = candidateChannel;
            std::memcpy(bestBssid, candidateBssid, sizeof(bestBssid));
        }
    }

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_AP_SCAN_TARGET_MATCHES count=");
    Serial.println(targetCount);

    if (!foundTarget) {
        handleAccessPointScanFailure(now, "WIFI_AP_SELECTION_NO_MATCH");
        return;
    }

    WiFi.scanDelete();

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_AP_SELECTED ssid=");
    Serial.print(_credentials.ssid);
    Serial.print(" bssid="); printBssid(bestBssid);
    Serial.print(" channel="); Serial.print(bestChannel);
    Serial.print(" rssi="); Serial.print(bestRssi);
    Serial.println(" dBm");

    _apSelectionActive = false;
    _apScanSettlePending = false;
    _apScanActive = false;
    _apScanRetryPending = false;
    _apScanFailureCount = 0;

    connectToAccessPoint(bestChannel, bestBssid, bestRssi);
}

void NetworkManager::handleAccessPointScanFailure(
    unsigned long now,
    const char* eventName
)
{
    WiFi.scanDelete();
    if (_apScanFailureCount < 255) ++_apScanFailureCount;

    Serial.print('['); Serial.print(now); Serial.print(" ms] ");
    Serial.print(eventName);
    Serial.print(" failure=");
    Serial.println(_apScanFailureCount);

    if (_apScanFailureCount < WIFI_AP_SCAN_MAX_FAILURES) {
        _apScanRetryPending = true;
        _apScanRetryRequestedMs = now;
        Serial.print('['); Serial.print(now);
        Serial.print(" ms] WIFI_AP_SCAN_RETRY_SCHEDULED delay=");
        Serial.print(WIFI_AP_SCAN_RETRY_DELAY_MS);
        Serial.print(" ms nextAttempt=");
        Serial.println(_apScanFailureCount + 1);
        return;
    }

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_AP_SCAN_GIVE_UP failures=");
    Serial.print(_apScanFailureCount);
    Serial.println(" fallback=automatic");

    _apSelectionActive = false;
    _apScanSettlePending = false;
    _apScanActive = false;
    _apScanRetryPending = false;
    _apScanFailureCount = 0;

    connectAutomatically("scan_failure_fallback");
}

void NetworkManager::connectToAccessPoint(
    int32_t channel,
    const uint8_t* bssid,
    int32_t targetRssi
)
{
    const unsigned long now = millis();

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_CONNECT_SELECTED ssid=");
    Serial.print(_credentials.ssid);
    Serial.print(" bssid="); printBssid(bssid);
    Serial.print(" channel="); Serial.print(channel);
    Serial.print(" scanRssi="); Serial.print(targetRssi);
    Serial.println(" dBm");

    _lastConnectionAttempt = now;
    _connectionStarted = true;

    WiFi.begin(
        _credentials.ssid.c_str(),
        _credentials.password.c_str(),
        channel,
        bssid,
        true
    );
}

void NetworkManager::connectAutomatically(const char* reason)
{
    const unsigned long now = millis();
    configureFallbackAccessPointSelection();

    Serial.print('['); Serial.print(now);
    Serial.print(" ms] WIFI_CONNECT_FALLBACK reason=");
    Serial.print(reason == nullptr ? "unknown" : reason);
    Serial.print(" ssid="); Serial.println(_credentials.ssid);

    _lastConnectionAttempt = now;
    _connectionStarted = true;
    WiFi.begin(_credentials.ssid.c_str(), _credentials.password.c_str());
}

void NetworkManager::cancelAccessPointSelection()
{
    const bool hadSelectionWork = _apSelectionActive;
    if (_apScanActive) WiFi.scanDelete();

    _apSelectionActive = false;
    _apScanSettlePending = false;
    _apScanActive = false;
    _apScanRetryPending = false;
    _apScanFailureCount = 0;
    _apSelectionReason = "";

    if (hadSelectionWork) {
        Serial.println("NetworkManager: access point selection cancelled.");
    }
}

void NetworkManager::logConnectionFailure(int status, unsigned long now)
{
    Serial.print('['); Serial.print(now); Serial.print(" ms] WIFI_CONNECT_FAILED status=");
    Serial.print(status); Serial.print(" name="); Serial.print(wifiStatusName(status));
    Serial.print(" attemptAgeMs="); Serial.println(now - _lastConnectionAttempt);
}
