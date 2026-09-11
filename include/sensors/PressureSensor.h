#pragma once

#include "sensors/PressureBubbleDetector.h"

class PressureSensor
{
public:
    PressureSensor();

    void begin();
    void update();
    void onSessionRunning();
    void onSessionPaused();

    bool isAvailable() const;
    float getPressurePa() const;

private:
    bool _available = false;
    float _pressurePa = 0.0f;
    PressureBubbleDetector _bubbleDetector;
};
