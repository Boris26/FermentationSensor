#pragma once

#include "session/MeasurementSession.h"

class MeasurementButton;
class MeasurementTransport;
class FermentationStarter;
class PressureSensor;
class StatusController;
class TemperatureSensor;
class TemperatureTransmissionPolicy;

class SensorSessionCoordinator
{
public:
    SensorSessionCoordinator(
        TemperatureSensor& temperatureSensor,
        PressureSensor& pressureSensor,
        MeasurementButton& button,
        MeasurementSession& session,
        TemperatureTransmissionPolicy& temperaturePolicy,
        MeasurementTransport& transport,
        FermentationStarter& fermentationStarter,
        StatusController& status
    );

    void begin();
    void updateInput();
    void updateMeasurements(bool sequenceReady);
    void updateSessionInput();
    bool sensorsReady() const { return _sensorsReady; }

private:
    void updateSensorInitialization();
    void initializeSessionIfSensorsReady();
    void queueTemperatureIfEligible(bool sequenceReady, bool newMeasurement);

    TemperatureSensor& _temperatureSensor;
    PressureSensor& _pressureSensor;
    MeasurementButton& _button;
    MeasurementSession& _session;
    TemperatureTransmissionPolicy& _temperaturePolicy;
    MeasurementTransport& _transport;
    FermentationStarter& _fermentationStarter;
    StatusController& _status;
    MeasurementState _lastState = MeasurementState::IDLE;
    bool _sensorsReady = false;
    bool _sessionInitialized = false;
    unsigned long _lastSensorCheckMs = 0;
};
