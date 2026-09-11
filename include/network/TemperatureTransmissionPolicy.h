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

    void recordSuccessfulSend(
        float beerTemperature,
        float ambientTemperature
    );

    void requestCurrentMeasurement();

private:
    float _sendDeltaC;
    float _lastSentBeerTemperature = 0.0f;
    float _lastSentAmbientTemperature = 0.0f;
    bool _hasSuccessfulSend = false;
    bool _currentMeasurementRequested = false;
};
