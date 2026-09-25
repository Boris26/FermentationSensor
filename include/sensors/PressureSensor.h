#pragma once

#include "sensors/PressureBubbleDetector.h"
#include "sensors/BubbleActivityAggregator.h"
#include "sensors/Lwlp5000Driver.h"
#include "config/SensorConfig.h"

class PressureSensor
{
public:
    PressureSensor();

    void begin();
    void update();
    void onSessionRunning();
    void onSessionPaused();
    void onSessionStopped();
    void applyConfig(const SensorConfig& config);

    bool isAvailable() const;
    float getPressurePa() const;
    bool hasReadAttempted() const;
    bool isLastReadValid() const;
    const char* getLastReadErrorName() const;
    bool hasCompletedBubbleActivityWindow() const;
    const BubbleActivityWindow& completedBubbleActivityWindow() const;
    void acknowledgeCompletedBubbleActivityWindow();

private:
    bool _available = false;
    bool _readAttempted = false;
    bool _lastReadValid = false;
    Lwlp5000ReadError _lastReadError = Lwlp5000ReadError::NOT_INITIALIZED;
    float _pressurePa = 0.0f;
    Lwlp5000Driver _driver;
    PressureBubbleDetector _bubbleDetector;
    BubbleActivityAggregator _bubbleActivityAggregator;
    uint32_t _diagnosedWindowRevision = 0;
    SensorConfig _config = SensorConfig::defaults();
    unsigned long _lastReadMs = 0;
};