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
    void turnMeasurementLedsOff();
    void update(bool sequenceReady, bool sensorsReady);

private:
    enum class ErrorState { NONE, STORAGE, SENSOR, NETWORK, BACKEND };
    StatusLed& _statusLed;
    StatusLed& _sessionLed;
    ErrorLed& _errorLed;
    NetworkManager& _networkManager;
    ServerClient& _serverClient;
    ErrorState _lastErrorState = ErrorState::NONE;
};
