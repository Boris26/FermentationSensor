#pragma once

#include "session/MeasurementSession.h"

class ErrorLed;
class NetworkManager;
class ServerClient;
class StatusLed;

class StatusController
{
public:
    StatusController(
        StatusLed& statusLed,
        StatusLed& sessionLed,
        ErrorLed& errorLed,
        NetworkManager& networkManager,
        ServerClient& serverClient
    );

    void begin();
    void showMeasurementState(MeasurementState state);
    void update(bool sequenceReady, bool sensorsReady);

private:
    enum class ErrorState { NONE, STORAGE, SENSOR, NETWORK, BACKEND };
    enum class LedMode { INITIALIZING, OFF, IDLE, RUNNING_BLINK, PAUSED_BLINK };
    void setMeasurementDisplayAvailable(bool available);
    void applyMeasurementLedMode();
    static const char* measurementStateName(MeasurementState state);
    static const char* ledModeName(LedMode mode);
    StatusLed& _statusLed;
    StatusLed& _sessionLed;
    ErrorLed& _errorLed;
    NetworkManager& _networkManager;
    ServerClient& _serverClient;
    ErrorState _lastErrorState = ErrorState::NONE;
    MeasurementState _measurementState = MeasurementState::IDLE;
    LedMode _effectiveLedMode = LedMode::INITIALIZING;
    bool _measurementDisplayAvailable = false;
};
