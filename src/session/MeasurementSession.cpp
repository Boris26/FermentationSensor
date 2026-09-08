#include "session/MeasurementSession.h"

#include <Arduino.h>

void MeasurementSession::begin()
{
    setState(
        MeasurementState::IDLE
    );
}

void MeasurementSession::handleButtonPress()
{
    switch (_state) {
        case MeasurementState::IDLE:
            setState(
                MeasurementState::RUNNING
            );
            break;

        case MeasurementState::RUNNING:
            setState(
                MeasurementState::PAUSED
            );
            break;

        case MeasurementState::PAUSED:
            setState(
                MeasurementState::RUNNING
            );
            break;
    }
}

MeasurementState
MeasurementSession::getState() const
{
    return _state;
}

bool MeasurementSession::isActive() const
{
    return _state !=
           MeasurementState::IDLE;
}

bool MeasurementSession::isRunning() const
{
    return _state ==
           MeasurementState::RUNNING;
}

bool MeasurementSession::isPaused() const
{
    return _state ==
           MeasurementState::PAUSED;
}

void MeasurementSession::setState(
    MeasurementState state
)
{
    _state = state;

    switch (_state) {
        case MeasurementState::IDLE:
            Serial.println(
                "Measurement state: IDLE"
            );
            break;

        case MeasurementState::RUNNING:
            Serial.println(
                "Measurement state: RUNNING"
            );
            break;

        case MeasurementState::PAUSED:
            Serial.println(
                "Measurement state: PAUSED"
            );
            break;
    }
}