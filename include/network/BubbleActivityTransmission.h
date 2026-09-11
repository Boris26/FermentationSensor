#pragma once

#include <stdint.h>

#include "sensors/BubbleActivityAggregator.h"

struct PendingBubbleActivity
{
    uint32_t sequence = 0;
    uint16_t bubbleCount = 0;
    uint32_t windowSeconds = 0;
};

// Owns exactly one copied activity window until the gateway acknowledges it.
// This is deliberately a transport slot, not an offline queue.
class BubbleActivityTransmission
{
public:
    explicit BubbleActivityTransmission(
        unsigned long acknowledgementTimeoutMs,
        uint32_t firstSequence = 1
    );

    bool accept(const BubbleActivityWindow& window);
    bool hasPending() const;
    const PendingBubbleActivity& pending() const;
    bool shouldSend(
        bool connected,
        bool registered,
        unsigned long nowMs
    ) const;
    bool isRetry() const;
    void recordSuccessfulSend(unsigned long nowMs);
    void onTransportUnavailable();
    bool acknowledge(uint32_t sequence);

private:
    const unsigned long _acknowledgementTimeoutMs;
    uint32_t _nextSequence;
    PendingBubbleActivity _pending;
    unsigned long _lastSentAtMs = 0;
    bool _hasPending = false;
    bool _hasBeenSent = false;
    bool _sendDue = false;
};
