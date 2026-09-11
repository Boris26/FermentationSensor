#pragma once

#include <stdint.h>

struct BubbleActivityWindow
{
    unsigned long startedAtMs = 0;
    unsigned long durationMs = 0;
    uint16_t bubbleCount = 0;
};

// Aggregates technical bubble events using active RUNNING time only. A fixed
// single result slot is used; if it is not consumed, the latest window wins.
class BubbleActivityAggregator
{
public:
    explicit BubbleActivityAggregator(unsigned long windowDurationMs);

    void start(unsigned long nowMs);
    void update(unsigned long nowMs);
    void pause(unsigned long nowMs);
    void resume(unsigned long nowMs);
    void reset();
    void recordBubble();

    bool isActive() const;
    bool isPaused() const;
    bool hasCompletedWindow() const;
    const BubbleActivityWindow& completedWindow() const;
    void acknowledgeCompletedWindow();
    uint32_t completedWindowRevision() const;

private:
    void completeWindow(unsigned long nextWindowStartedAtMs);

    const unsigned long _windowDurationMs;
    BubbleActivityWindow _currentWindow;
    BubbleActivityWindow _completedWindow;
    unsigned long _activeElapsedMs = 0;
    unsigned long _lastUpdateMs = 0;
    bool _active = false;
    bool _paused = true;
    bool _hasCompletedWindow = false;
    uint32_t _completedWindowRevision = 0;
};
