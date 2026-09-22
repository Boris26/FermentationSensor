#pragma once

#include <Arduino.h>

#include "network/WifiCredentials.h"

class NetworkManager
{
public:
    void begin(const WifiCredentials& credentials);
    void update();

    bool isConnected() const;
    bool isRoaming() const { return _apSelectionActive; }
    void requestReconnect(const char* reason);

private:
    void scheduleAccessPointSelection(
        unsigned long now,
        const char* reason,
        bool disconnectCurrent
    );
    void updateAccessPointSelection(unsigned long now);
    void startAccessPointScan(unsigned long now);
    void finishAccessPointScan(int16_t networkCount, unsigned long now);
    void handleAccessPointScanFailure(
        unsigned long now,
        const char* eventName
    );
    void connectToAccessPoint(
        int32_t channel,
        const uint8_t* bssid,
        int32_t targetRssi
    );
    void connectAutomatically(const char* reason);
    void cancelAccessPointSelection();
    void logConnectionFailure(int wifiStatus, unsigned long now);

    WifiCredentials _credentials;

    unsigned long _lastConnectionAttempt = 0;
    bool _connectionStarted = false;
    bool _restartPending = false;
    unsigned long _restartRequestedMs = 0;
    unsigned long _lastRssiLogMs = 0;
    unsigned long _lastRoamCheckMs = 0;

    bool _apSelectionActive = false;
    bool _apScanSettlePending = false;
    unsigned long _apScanSettleStartedMs = 0;
    bool _apScanRetryPending = false;
    unsigned long _apScanRetryRequestedMs = 0;
    uint8_t _apScanFailureCount = 0;
    String _apSelectionReason;

    int _lastReportedStatus = -1;
};
