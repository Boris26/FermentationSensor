#pragma once

#include <Arduino.h>

#include "network/WifiCredentials.h"

class NetworkManager
{
public:
    void begin(const WifiCredentials& credentials);
    void update();

    bool isConnected() const;
    bool isRoaming() const
    {
        return _roamScanActive || _roamSettlePending || _roamRetryPending;
    }
    void requestReconnect(const char* reason);

private:
    void connect();
    void connectToAccessPoint(
        int32_t channel,
        const uint8_t* bssid,
        int32_t targetRssi
    );
    void updateRoaming(unsigned long now);
    void startRoamingScan(unsigned long now, int32_t currentRssi);
    void finishRoamingScan(int16_t networkCount, unsigned long now);
    void handleRoamingScanFailure(unsigned long now, const char* eventName);
    void cancelRoamingScan();
    void logConnectionFailure(int wifiStatus, unsigned long now);

    WifiCredentials _credentials;

    unsigned long _lastConnectionAttempt = 0;
    bool _connectionStarted = false;
    bool _restartPending = false;
    unsigned long _restartRequestedMs = 0;
    unsigned long _lastRssiLogMs = 0;
    unsigned long _lastRoamCheckMs = 0;
    bool _roamScanActive = false;
    bool _roamSettlePending = false;
    unsigned long _roamSettleStartedMs = 0;
    bool _roamRetryPending = false;
    unsigned long _roamRetryRequestedMs = 0;
    uint8_t _roamScanFailureCount = 0;
    int _lastReportedStatus = -1;
};
