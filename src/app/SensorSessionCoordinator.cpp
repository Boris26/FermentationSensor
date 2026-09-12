#include <Arduino.h>

#include "app/SensorSessionCoordinator.h"
#include "app/MeasurementTransport.h"
#include "app/StatusController.h"
#include "config/Config.h"
#include "input/MeasurementButton.h"
#include "network/TemperatureTransmissionPolicy.h"
#include "sensors/PressureSensor.h"
#include "sensors/TemperatureSensor.h"

SensorSessionCoordinator::SensorSessionCoordinator(
    TemperatureSensor& temperatureSensor, PressureSensor& pressureSensor,
    MeasurementButton& button, MeasurementSession& session,
    TemperatureTransmissionPolicy& temperaturePolicy,
    MeasurementTransport& transport, StatusController& status
) : _temperatureSensor(temperatureSensor), _pressureSensor(pressureSensor),
    _button(button), _session(session), _temperaturePolicy(temperaturePolicy),
    _transport(transport), _status(status)
{
}

void SensorSessionCoordinator::begin()
{
    _pressureSensor.begin();
    _temperatureSensor.begin();
    _button.begin();
}

void SensorSessionCoordinator::initializeSessionIfSensorsReady()
{
    if (!_temperatureSensor.areAllSensorsConnected()) {
        _sensorsReady = false;
        _status.turnMeasurementLedsOff();
        Serial.println("Temperature sensors not ready.");
        return;
    }
    _sensorsReady = true;
    if (!_sessionInitialized) {
        _session.begin();
        _sessionInitialized = true;
        _lastState = _session.getState();
        _status.showMeasurementState(_lastState);
        Serial.println("Temperature sensors ready.");
        Serial.println("Measurement session initialized.");
    }
}

void SensorSessionCoordinator::updateSensorInitialization()
{
    if (_temperatureSensor.isConversionInProgress()) return;
    const unsigned long now = millis();
    if (now - _lastSensorCheckMs < SENSOR_CHECK_INTERVAL_MS) return;
    _lastSensorCheckMs = now;
    _temperatureSensor.refresh();
    initializeSessionIfSensorsReady();
}

void SensorSessionCoordinator::queueTemperatureIfEligible(
    bool sequenceReady, bool newMeasurement
)
{
    if (!sequenceReady || !newMeasurement || !_sessionInitialized ||
        !_sensorsReady || !_session.isRunning() ||
        !_temperaturePolicy.shouldSend(
            _temperatureSensor.getBeerTemperature(),
            _temperatureSensor.getAmbientTemperature()
        )) return;

    const float beer = _temperatureSensor.getBeerTemperature();
    const float ambient = _temperatureSensor.getAmbientTemperature();
    const bool equivalentReconnectSnapshot =
        _temperaturePolicy.isCurrentMeasurementRequested() &&
        _transport.lastTemperatureIsEquivalent(beer, ambient);
    if (equivalentReconnectSnapshot || _transport.bufferTemperature(
        beer, ambient, _pressureSensor.isAvailable(),
        _pressureSensor.getPressurePa(), static_cast<uint32_t>(millis())
    )) {
        _temperaturePolicy.recordQueuedMeasurement(beer, ambient);
    }
}

void SensorSessionCoordinator::updateMeasurements(bool sequenceReady)
{
    const bool newMeasurement = _temperatureSensor.update();
    updateSensorInitialization();
    queueTemperatureIfEligible(sequenceReady, newMeasurement);
}

void SensorSessionCoordinator::updateInput()
{
    _button.update();
}

void SensorSessionCoordinator::updateSessionInput()
{
    if (_sessionInitialized && _button.wasPressed()) {
        _session.handleButtonPress();
        const MeasurementState current = _session.getState();
        if (current == MeasurementState::RUNNING) _pressureSensor.onSessionRunning();
        else if (current == MeasurementState::PAUSED) _pressureSensor.onSessionPaused();
        if (current != _lastState) {
            _status.showMeasurementState(current);
            _lastState = current;
        }
    }
    if (_sessionInitialized && _session.isRunning()) _pressureSensor.update();
}
