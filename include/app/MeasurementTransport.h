#pragma once

#include <stdint.h>

class MeasurementOutbox;
class PressureSensor;
class ServerClient;

class MeasurementTransport
{
public:
    MeasurementTransport(
        ServerClient& serverClient,
        MeasurementOutbox& outbox,
        PressureSensor& pressureSensor
    );

    void update(bool sequenceReady);
    bool bufferTemperature(
        float beerTemperature,
        float ambientTemperature,
        bool pressureAvailable,
        float pressurePa,
        uint32_t measuredAtMs
    );
    bool lastTemperatureIsEquivalent(float beer, float ambient) const;
    void resetRuntimeState();

private:
    void printOverflowIfChanged(uint32_t previousDroppedCount) const;
    void printBuffered() const;

    ServerClient& _serverClient;
    MeasurementOutbox& _outbox;
    PressureSensor& _pressureSensor;
};
