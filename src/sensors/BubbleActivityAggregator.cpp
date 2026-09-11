#include "sensors/BubbleActivityAggregator.h"

#include <limits.h>

BubbleActivityAggregator::BubbleActivityAggregator(unsigned long windowDurationMs)
    : _windowDurationMs(windowDurationMs)
{
}

void BubbleActivityAggregator::start(unsigned long nowMs)
{
    _currentWindow = BubbleActivityWindow();
    _currentWindow.startedAtMs = nowMs;
    _currentWindow.durationMs = _windowDurationMs;
    _activeElapsedMs = 0;
    _lastUpdateMs = nowMs;
    _pressureDeltaSum = 0.0f;
    _pressureSampleCount = 0;
    _active = true;
    _paused = false;
}

void BubbleActivityAggregator::update(unsigned long nowMs)
{
    if (!_active || _paused || _windowDurationMs == 0) return;

    unsigned long elapsed = nowMs - _lastUpdateMs;
    _lastUpdateMs = nowMs;

    while (elapsed >= _windowDurationMs - _activeElapsedMs) {
        const unsigned long remaining = _windowDurationMs - _activeElapsedMs;
        elapsed -= remaining;
        completeWindow(nowMs - elapsed);
    }
    _activeElapsedMs += elapsed;
}

void BubbleActivityAggregator::pause(unsigned long nowMs)
{
    if (!_active || _paused) return;
    update(nowMs);
    _paused = true;
}

void BubbleActivityAggregator::resume(unsigned long nowMs)
{
    if (!_active || !_paused) return;
    _lastUpdateMs = nowMs;
    _paused = false;
}

void BubbleActivityAggregator::reset()
{
    _currentWindow = BubbleActivityWindow();
    _completedWindow = BubbleActivityWindow();
    _activeElapsedMs = 0;
    _lastUpdateMs = 0;
    _pressureDeltaSum = 0.0f;
    _pressureSampleCount = 0;
    _active = false;
    _paused = true;
    _hasCompletedWindow = false;
    _completedWindowRevision = 0;
}

void BubbleActivityAggregator::recordBubble()
{
    if (!_active || _paused) return;
    if (_currentWindow.bubbleCount < UINT16_MAX) ++_currentWindow.bubbleCount;
}

void BubbleActivityAggregator::recordPressureDelta(float pressureDeltaPa)
{
    if (!_active || _paused) return;
    _pressureDeltaSum += pressureDeltaPa;
    ++_pressureSampleCount;
}

bool BubbleActivityAggregator::isActive() const { return _active; }
bool BubbleActivityAggregator::isPaused() const { return _paused; }
bool BubbleActivityAggregator::hasCompletedWindow() const { return _hasCompletedWindow; }

const BubbleActivityWindow& BubbleActivityAggregator::completedWindow() const
{
    return _completedWindow;
}

void BubbleActivityAggregator::acknowledgeCompletedWindow()
{
    _hasCompletedWindow = false;
}

uint32_t BubbleActivityAggregator::completedWindowRevision() const
{
    return _completedWindowRevision;
}

void BubbleActivityAggregator::completeWindow(unsigned long nextWindowStartedAtMs)
{
    _completedWindow = _currentWindow;
    _completedWindow.pressureSampleCount = _pressureSampleCount;
    if (_pressureSampleCount > 0) {
        _completedWindow.averagePressureDeltaPa =
            _pressureDeltaSum / static_cast<float>(_pressureSampleCount);
    }
    _completedWindow.completedAtMs = nextWindowStartedAtMs;
    _hasCompletedWindow = true;
    ++_completedWindowRevision;
    _currentWindow = BubbleActivityWindow();
    _currentWindow.startedAtMs = nextWindowStartedAtMs;
    _currentWindow.durationMs = _windowDurationMs;
    _pressureDeltaSum = 0.0f;
    _pressureSampleCount = 0;
    _activeElapsedMs = 0;
}
