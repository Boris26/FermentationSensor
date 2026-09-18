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

const char* StatusController::measurementStateName(MeasurementState state)
{
    switch (state) {
        case MeasurementState::IDLE: return "IDLE";
        case MeasurementState::RUNNING: return "RUNNING";
        case MeasurementState::PAUSED: return "PAUSED";
    }
    return "UNKNOWN";
}

void StatusController::showMeasurementState(MeasurementState state)
{
    if (state != _measurementState) {
        Serial.print("STATUS_CHANGED old=");
        Serial.print(measurementStateName(_measurementState));
        Serial.print(" new=");
        Serial.println(measurementStateName(state));
    }
    _measurementState = state;
    applyMeasurementLedMode();
}

const char* StatusController::ledModeName(LedMode mode)
{
    switch (mode) {
        case LedMode::INITIALIZING: return "INITIALIZING";
        case LedMode::OFF: return "OFF";
        case LedMode::IDLE: return "IDLE";
        case LedMode::RUNNING_BLINK: return "RUNNING_BLINK";
        case LedMode::PAUSED_BLINK: return "PAUSED_BLINK";
    }
    return "UNKNOWN";
}

void StatusController::setMeasurementDisplayAvailable(bool available)
{
    if (available == _measurementDisplayAvailable) return;

    _measurementDisplayAvailable = available;
    Serial.print("LED_OVERRIDE_CHANGED source=temperature_sensors active=");
    Serial.println(available ? "false" : "true");
    applyMeasurementLedMode();
}

void StatusController::applyMeasurementLedMode()
{
    LedMode nextMode = LedMode::OFF;
    if (_measurementDisplayAvailable) {
        switch (_measurementState) {
            case MeasurementState::IDLE: nextMode = LedMode::IDLE; break;
            case MeasurementState::RUNNING:
                nextMode = LedMode::RUNNING_BLINK; break;
            case MeasurementState::PAUSED:
                nextMode = LedMode::PAUSED_BLINK; break;
        }
    }

    if (nextMode == _effectiveLedMode) return;

    Serial.print("LED_MODE_CHANGED old=");
    Serial.print(ledModeName(_effectiveLedMode));
    Serial.print(" new=");
    Serial.print(ledModeName(nextMode));
    Serial.print(" reason=");
    Serial.println(_measurementDisplayAvailable ? "measurement_state" :
        "temperature_sensors_unavailable");

    _effectiveLedMode = nextMode;
    switch (nextMode) {
        case LedMode::INITIALIZING:
            break;
        case LedMode::OFF:
            _statusLed.off();
            _sessionLed.off();
            break;
        case LedMode::IDLE:
            _statusLed.on();
            _sessionLed.off();
            break;
        case LedMode::RUNNING_BLINK:
            _statusLed.off();
            _sessionLed.startBlinking(SESSION_LED_BLINK_INTERVAL_MS);
            break;
        case LedMode::PAUSED_BLINK:
            _statusLed.startBlinking(STATUS_LED_BLINK_INTERVAL_MS);
            _sessionLed.startBlinking(SESSION_LED_BLINK_INTERVAL_MS);
            break;
    }
}

void StatusController::update(bool sequenceReady, bool sensorsReady)
{
    setMeasurementDisplayAvailable(sensorsReady);
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
