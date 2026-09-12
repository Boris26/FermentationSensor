#include "app/StatusController.h"
#include "config/Config.h"
#include "network/NetworkManager.h"
#include "network/ServerClient.h"
#include "output/ErrorLed.h"
#include "output/StatusLed.h"

StatusController::StatusController(
    StatusLed& statusLed, StatusLed& sessionLed, ErrorLed& errorLed,
    NetworkManager& networkManager, ServerClient& serverClient
) : _statusLed(statusLed), _sessionLed(sessionLed), _errorLed(errorLed),
    _networkManager(networkManager), _serverClient(serverClient)
{
}

void StatusController::begin()
{
    _statusLed.begin();
    _sessionLed.begin();
    _errorLed.begin();
    _statusLed.startBlinking(INIT_LED_BLINK_INTERVAL_MS);
    _sessionLed.off();
    _errorLed.off();
}

void StatusController::turnMeasurementLedsOff()
{
    _statusLed.off();
    _sessionLed.off();
}

void StatusController::showMeasurementState(MeasurementState state)
{
    switch (state) {
        case MeasurementState::IDLE:
            _statusLed.on();
            _sessionLed.off();
            break;
        case MeasurementState::RUNNING:
            _statusLed.off();
            _sessionLed.startBlinking(SESSION_LED_BLINK_INTERVAL_MS);
            break;
        case MeasurementState::PAUSED:
            _statusLed.startBlinking(STATUS_LED_BLINK_INTERVAL_MS);
            _sessionLed.startBlinking(SESSION_LED_BLINK_INTERVAL_MS);
            break;
    }
}

void StatusController::update(bool sequenceReady, bool sensorsReady)
{
    ErrorState current = ErrorState::NONE;
    if (!sequenceReady) current = ErrorState::STORAGE;
    else if (!sensorsReady) current = ErrorState::SENSOR;
    else if (!_networkManager.isConnected()) current = ErrorState::NETWORK;
    else if (!_serverClient.isConnected() || !_serverClient.isRegistered()) {
        current = ErrorState::BACKEND;
    }

    if (current != _lastErrorState) {
        _lastErrorState = current;
        switch (current) {
            case ErrorState::NONE: _errorLed.off(); break;
            case ErrorState::STORAGE: _errorLed.on(); break;
            case ErrorState::SENSOR:
                _errorLed.startBlinking(ERROR_LED_BLINK_INTERVAL_MS); break;
            case ErrorState::NETWORK:
                _errorLed.startBlinking(NETWORK_ERROR_LED_BLINK_INTERVAL_MS); break;
            case ErrorState::BACKEND:
                _errorLed.startBlinking(BACKEND_ERROR_LED_BLINK_INTERVAL_MS); break;
        }
    }
    _statusLed.update();
    _sessionLed.update();
    _errorLed.update();
}
