#pragma once

#include "sensors/PressureBubbleDetector.h"
#include "sensors/BubbleActivityAggregator.h"

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
    bool hasCompletedBubbleActivityWindow() const;
    const BubbleActivityWindow& completedBubbleActivityWindow() const;
    void acknowledgeCompletedBubbleActivityWindow();

private:
    bool _available = false;
    float _pressurePa = 0.0f;
    PressureBubbleDetector _bubbleDetector;
    BubbleActivityAggregator _bubbleActivityAggregator;
    uint32_t _diagnosedWindowRevision = 0;
};
