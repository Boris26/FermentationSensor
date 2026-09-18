#include <Arduino.h>

#include "app/SensorSessionCoordinator.h"
#include "app/MeasurementTransport.h"
#include "app/StatusController.h"
#include "config/Config.h"
#include "input/MeasurementButton.h"
#include "network/TemperatureTransmissionPolicy.h"
#include "network/ServerClient.h"
#include "sensors/PressureSensor.h"
#include "sensors/TemperatureSensor.h"

SensorSessionCoordinator::SensorSessionCoordinator(
    TemperatureSensor& temperatureSensor, PressureSensor& pressureSensor,
    MeasurementButton& button, MeasurementSession& session,
    TemperatureTransmissionPolicy& temperaturePolicy,
    MeasurementTransport& transport, StatusController& status,
    ServerClient& serverClient
) : _temperatureSensor(temperatureSensor), _pressureSensor(pressureSensor),
    _button(button), _session(session), _temperaturePolicy(temperaturePolicy),
    _transport(transport), _status(status), _serverClient(serverClient)
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
        !_sensorsReady || !_session.isRunning()) return;

    const bool forceInitialMeasurement = _initialMeasurementPending;
    if (!forceInitialMeasurement && !_temperaturePolicy.shouldSend(
            _temperatureSensor.getBeerTemperature(),
            _temperatureSensor.getAmbientTemperature()
        )) return;

    const float beer = _temperatureSensor.getBeerTemperature();
    const float ambient = _temperatureSensor.getAmbientTemperature();
    const bool equivalentReconnectSnapshot = !forceInitialMeasurement &&
        _temperaturePolicy.isCurrentMeasurementRequested() &&
        _transport.lastTemperatureIsEquivalent(beer, ambient);
    if (equivalentReconnectSnapshot || _transport.bufferTemperature(
        beer, ambient, _pressureSensor.isAvailable(),
        _pressureSensor.getPressurePa(), static_cast<uint32_t>(millis())
    )) {
        _temperaturePolicy.recordQueuedMeasurement(beer, ambient);
        _initialMeasurementPending = false;
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
    handleRegistrationEstablished();
    handleStopMeasurementRequests();
    handleMeasurementAssignments();
    if (_sessionInitialized && _button.wasPressed()) {
        _session.handleButtonPress();
        const MeasurementState current = _session.getState();
        if (current == MeasurementState::RUNNING) {
            _pressureSensor.onSessionRunning();
            _temperatureSensor.requestImmediateMeasurement();
            _initialMeasurementPending = true;
        }
        else if (current == MeasurementState::PAUSED) _pressureSensor.onSessionPaused();
        if (current != _lastState) {
            _status.showMeasurementState(current);
            publishMeasurementState(current);
            _lastState = current;
        }
    }
    if (_sessionInitialized && _session.isRunning()) _pressureSensor.update();
}

void SensorSessionCoordinator::handleRegistrationEstablished()
{
    if (!_serverClient.takeRegistrationEstablished()) return;

    const MeasurementState current = _session.getState();
    if (publishMeasurementState(current, true)) {
        Serial.print("Measurement runtime state resynchronized after registration: ");
        switch (current) {
            case MeasurementState::IDLE: Serial.println("IDLE"); break;
            case MeasurementState::RUNNING: Serial.println("RUNNING"); break;
            case MeasurementState::PAUSED: Serial.println("PAUSED"); break;
        }
    }
}

bool SensorSessionCoordinator::publishMeasurementState(
    MeasurementState state, bool force
)
{
    if (!force && _hasPublishedState && state == _lastPublishedState) return false;
    if (!_serverClient.sendMeasurementState(state)) return false;

    _lastPublishedState = state;
    _hasPublishedState = true;
    Serial.print("Measurement runtime state sent: ");
    switch (state) {
        case MeasurementState::IDLE: Serial.println("IDLE"); break;
        case MeasurementState::RUNNING: Serial.println("RUNNING"); break;
        case MeasurementState::PAUSED: Serial.println("PAUSED"); break;
    }
    return true;
}

void SensorSessionCoordinator::handleStopMeasurementRequests()
{
    while (_serverClient.takeStopMeasurementRequest()) {
        resetSessionRuntime(true);
        _serverClient.sendStopMeasurementAck();
        Serial.println("Measurement session stopped.");
    }
}

void SensorSessionCoordinator::handleMeasurementAssignments()
{
    String beerId;
    while (_serverClient.takeMeasurementAssignment(beerId)) {
        const bool unchangedIdleAssignment =
            _session.getState() == MeasurementState::IDLE &&
            _serverClient.finishedBeerId() == beerId;

        if (!unchangedIdleAssignment) {
            // Also reset an IDLE assignment change: detector and buffered
            // measurement history must never cross the beer boundary.
            resetSessionRuntime(true);
            if (!_serverClient.assignFinishedBeerContext(beerId)) continue;
        }

        _serverClient.sendAssignMeasurementAck(beerId);
        Serial.println("Measurement assignment accepted; session remains IDLE.");
    }
}

void SensorSessionCoordinator::resetSessionRuntime(bool clearAssignment)
{
    const MeasurementState previousState = _session.getState();
    _session.stop();
    _transport.resetRuntimeState();
    _temperaturePolicy.resetRuntimeState();
    _pressureSensor.onSessionStopped();
    _temperatureSensor.cancelImmediateMeasurementRequest();
    _initialMeasurementPending = false;
    if (clearAssignment) _serverClient.clearFinishedBeerContext();
    _lastState = MeasurementState::IDLE;
    _status.showMeasurementState(MeasurementState::IDLE);
    if (previousState != MeasurementState::IDLE) {
        publishMeasurementState(MeasurementState::IDLE);
    }
}
