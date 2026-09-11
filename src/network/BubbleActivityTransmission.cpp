#include "network/BubbleActivityTransmission.h"

BubbleActivityTransmission::BubbleActivityTransmission(
    unsigned long acknowledgementTimeoutMs,
    uint32_t firstSequence
)
    : _acknowledgementTimeoutMs(acknowledgementTimeoutMs),
      _nextSequence(firstSequence)
{
}

bool BubbleActivityTransmission::accept(
    const BubbleActivityWindow& window
)
{
    if (_hasPending) return false;

    _pending.sequence = _nextSequence++;
    _pending.bubbleCount = window.bubbleCount;
    _pending.windowSeconds =
        static_cast<uint32_t>(window.durationMs / 1000UL);
    _hasPending = true;
    _hasBeenSent = false;
    _sendDue = true;
    return true;
}

bool BubbleActivityTransmission::hasPending() const
{
    return _hasPending;
}

const PendingBubbleActivity&
BubbleActivityTransmission::pending() const
{
    return _pending;
}

bool BubbleActivityTransmission::shouldSend(
    bool connected,
    bool registered,
    unsigned long nowMs
) const
{
    if (!_hasPending || !connected || !registered) return false;
    return _sendDue ||
        (_hasBeenSent &&
         nowMs - _lastSentAtMs >= _acknowledgementTimeoutMs);
}

bool BubbleActivityTransmission::isRetry() const
{
    return _hasBeenSent;
}

void BubbleActivityTransmission::recordSuccessfulSend(
    unsigned long nowMs
)
{
    if (!_hasPending) return;
    _lastSentAtMs = nowMs;
    _hasBeenSent = true;
    _sendDue = false;
}

void BubbleActivityTransmission::onTransportUnavailable()
{
    if (_hasPending && _hasBeenSent) _sendDue = true;
}

bool BubbleActivityTransmission::acknowledge(uint32_t sequence)
{
    if (!_hasPending || _pending.sequence != sequence) return false;
    _pending = PendingBubbleActivity();
    _hasPending = false;
    _hasBeenSent = false;
    _sendDue = false;
    return true;
}
