#include "network/TemperatureTransmissionPolicy.h"

#include <cmath>


TemperatureTransmissionPolicy::TemperatureTransmissionPolicy(
    float sendDeltaC
)
    : _sendDeltaC(sendDeltaC)
{
}


bool TemperatureTransmissionPolicy::shouldSend(
    float beerTemperature,
    float ambientTemperature
) const
{
    if (
        !_hasQueuedMeasurement ||
        _currentMeasurementRequested
    ) {
        return true;
    }

    return
        std::fabs(
            beerTemperature -
            _lastQueuedBeerTemperature
        ) >= _sendDeltaC ||
        std::fabs(
            ambientTemperature -
            _lastQueuedAmbientTemperature
        ) >= _sendDeltaC;
}


void TemperatureTransmissionPolicy::recordQueuedMeasurement(
    float beerTemperature,
    float ambientTemperature
)
{
    _lastQueuedBeerTemperature =
        beerTemperature;

    _lastQueuedAmbientTemperature =
        ambientTemperature;

    _hasQueuedMeasurement = true;
    _currentMeasurementRequested = false;
}


void TemperatureTransmissionPolicy::requestCurrentMeasurement()
{
    _currentMeasurementRequested = true;
}

bool TemperatureTransmissionPolicy::isCurrentMeasurementRequested() const
{
    return _currentMeasurementRequested;
}
