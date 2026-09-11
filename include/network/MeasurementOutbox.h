#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config/Config.h"
#include "network/MeasurementSequenceAllocator.h"
#include "sensors/BubbleActivityAggregator.h"

static_assert(
    MEASUREMENT_OUTBOX_CAPACITY > 0,
    "Measurement outbox capacity must be greater than zero."
);

enum class MeasurementType : uint8_t
{
    TEMPERATURE,
    BUBBLE_ACTIVITY
};

struct TemperatureMeasurementPayload
{
    float beerTemperature;
    float ambientTemperature;
    float pressurePa;
    bool pressureAvailable;
};

struct BubbleActivityPayload
{
    uint16_t bubbleCount;
    uint32_t windowSeconds;
    float averagePressureDeltaPa;
};

struct OutboxEntry
{
    MeasurementType type;
    uint32_t sequence;
    uint32_t capturedAtMs;
    union
    {
        TemperatureMeasurementPayload temperature;
        BubbleActivityPayload bubbleActivity;
    } payload;

    uint32_t ageSeconds(uint32_t nowMs) const;
};

// Bounded, allocation-free FIFO for historical measurements. The oldest entry
// remains at the head until a matching gateway acknowledgement is received.
class MeasurementOutbox
{
public:
    explicit MeasurementOutbox(
        unsigned long acknowledgementTimeoutMs,
        MeasurementSequenceSource& sequenceSource
    );

    bool enqueueTemperature(
        float beerTemperature,
        float ambientTemperature,
        bool pressureAvailable,
        float pressurePa,
        uint32_t measuredAtMs
    );
    bool enqueueBubbleActivity(const BubbleActivityWindow& window);

    bool empty() const;
    bool full() const;
    size_t size() const;
    size_t capacity() const;
    const OutboxEntry& front() const;
    const OutboxEntry& back() const;
    bool backIsEquivalentTemperature(
        float beerTemperature,
        float ambientTemperature
    ) const;

    bool shouldSend(bool connected, bool registered, uint32_t nowMs) const;
    bool isRetry() const;
    void recordSuccessfulSend(uint32_t nowMs);
    void onTransportUnavailable();
    bool acknowledge(uint32_t sequence);

    uint32_t droppedCount() const;
    MeasurementType lastDroppedType() const;
    uint32_t lastDroppedSequence() const;

private:
    bool enqueue(const OutboxEntry& entry);
    void dropOldest();

    const unsigned long _acknowledgementTimeoutMs;
    MeasurementSequenceSource& _sequenceSource;
    OutboxEntry _entries[MEASUREMENT_OUTBOX_CAPACITY];
    size_t _head = 0;
    size_t _count = 0;
    uint32_t _lastSentAtMs = 0;
    uint32_t _droppedCount = 0;
    MeasurementType _lastDroppedType = MeasurementType::TEMPERATURE;
    uint32_t _lastDroppedSequence = 0;
    bool _headHasBeenSent = false;
    bool _sendDue = false;
};
