#pragma once

enum class MeasurementState
{
    IDLE,
    RUNNING,
    PAUSED
};

class MeasurementSession
{
public:
    void begin();

    void handleButtonPress();

    MeasurementState getState() const;

    bool isActive() const;
    bool isRunning() const;
    bool isPaused() const;

private:
    void setState(
        MeasurementState state
    );

    MeasurementState _state =
        MeasurementState::IDLE;
};