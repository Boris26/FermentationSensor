#pragma once


class TemperatureTransmissionPolicy
{
public:
    explicit TemperatureTransmissionPolicy(
        float sendDeltaC
    );

    bool shouldSend(
        float beerTemperature,
        float ambientTemperature
    ) const;

    void recordQueuedMeasurement(
        float beerTemperature,
        float ambientTemperature
    );

    void requestCurrentMeasurement();
    bool isCurrentMeasurementRequested() const;
    void resetRuntimeState();
    void setSendDeltaC(float sendDeltaC) { _sendDeltaC = sendDeltaC; }

private:
    float _sendDeltaC;
    float _lastQueuedBeerTemperature = 0.0f;
    float _lastQueuedAmbientTemperature = 0.0f;
    bool _hasQueuedMeasurement = false;
    bool _currentMeasurementRequested = false;
};
