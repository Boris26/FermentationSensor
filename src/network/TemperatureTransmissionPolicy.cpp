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
        !_hasSuccessfulSend ||
        _currentMeasurementRequested
    ) {
        return true;
    }

    return
        std::fabs(
            beerTemperature -
            _lastSentBeerTemperature
        ) >= _sendDeltaC ||
        std::fabs(
            ambientTemperature -
            _lastSentAmbientTemperature
        ) >= _sendDeltaC;
}


void TemperatureTransmissionPolicy::recordSuccessfulSend(
    float beerTemperature,
    float ambientTemperature
)
{
    _lastSentBeerTemperature =
        beerTemperature;

    _lastSentAmbientTemperature =
        ambientTemperature;

    _hasSuccessfulSend = true;
    _currentMeasurementRequested = false;
}


void TemperatureTransmissionPolicy::requestCurrentMeasurement()
{
    _currentMeasurementRequested = true;
}
