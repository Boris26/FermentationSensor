#include "network/MeasurementOutbox.h"

uint32_t OutboxEntry::ageSeconds(uint32_t nowMs) const
{
    return (nowMs - capturedAtMs) / 1000U;
}

MeasurementOutbox::MeasurementOutbox(
    unsigned long acknowledgementTimeoutMs,
    MeasurementSequenceSource& sequenceSource
)
    : _acknowledgementTimeoutMs(acknowledgementTimeoutMs),
      _sequenceSource(sequenceSource)
{
}

void MeasurementOutbox::resetRuntimeState()
{
    _head = 0;
    _count = 0;
    _lastSentAtMs = 0;
    _droppedCount = 0;
    _lastDroppedType = MeasurementType::TEMPERATURE;
    _lastDroppedSequence = 0;
    _headHasBeenSent = false;
    _sendDue = false;
}

bool MeasurementOutbox::enqueueTemperature(
    float beerTemperature,
    float ambientTemperature,
    bool pressureAvailable,
    float pressurePa,
    uint32_t measuredAtMs
)
{
    OutboxEntry entry = {};
    entry.type = MeasurementType::TEMPERATURE;
    entry.capturedAtMs = measuredAtMs;
    entry.payload.temperature.beerTemperature = beerTemperature;
    entry.payload.temperature.ambientTemperature = ambientTemperature;
    entry.payload.temperature.pressureAvailable = pressureAvailable;
    entry.payload.temperature.pressurePa = pressurePa;
    return enqueue(entry);
}

bool MeasurementOutbox::enqueueBubbleActivity(
    const BubbleActivityWindow& window
)
{
    // A window without a pressure sample has no meaningful average and must not
    // be represented as a measured 0 Pa value.
    if (window.pressureSampleCount == 0) return false;

    OutboxEntry entry = {};
    entry.type = MeasurementType::BUBBLE_ACTIVITY;
    entry.capturedAtMs = static_cast<uint32_t>(window.completedAtMs);
    entry.payload.bubbleActivity.bubbleCount = window.bubbleCount;
    entry.payload.bubbleActivity.windowSeconds =
        static_cast<uint32_t>(window.durationMs / 1000UL);
    entry.payload.bubbleActivity.averagePressureDeltaPa =
        window.averagePressureDeltaPa;
    return enqueue(entry);
}

bool MeasurementOutbox::enqueue(const OutboxEntry& source)
{
    uint32_t sequence = 0;
    if (!_sequenceSource.next(sequence)) return false;

    if (full()) dropOldest();

    const size_t tail = (_head + _count) % capacity();
    _entries[tail] = source;
    _entries[tail].sequence = sequence;
    ++_count;
    if (_count == 1) {
        _headHasBeenSent = false;
        _sendDue = true;
    }
    return true;
}

void MeasurementOutbox::dropOldest()
{
    const OutboxEntry& dropped = front();
    _lastDroppedType = dropped.type;
    _lastDroppedSequence = dropped.sequence;
    ++_droppedCount;
    _head = (_head + 1) % capacity();
    --_count;
    _headHasBeenSent = false;
    _sendDue = !empty();
}

bool MeasurementOutbox::empty() const { return _count == 0; }
bool MeasurementOutbox::full() const { return _count == capacity(); }
size_t MeasurementOutbox::size() const { return _count; }
size_t MeasurementOutbox::capacity() const { return MEASUREMENT_OUTBOX_CAPACITY; }

const OutboxEntry& MeasurementOutbox::front() const
{
    return _entries[_head];
}

const OutboxEntry& MeasurementOutbox::back() const
{
    return _entries[(_head + _count - 1) % capacity()];
}

bool MeasurementOutbox::backIsEquivalentTemperature(
    float beerTemperature,
    float ambientTemperature
) const
{
    if (empty() || back().type != MeasurementType::TEMPERATURE) return false;
    return back().payload.temperature.beerTemperature == beerTemperature &&
        back().payload.temperature.ambientTemperature == ambientTemperature;
}

bool MeasurementOutbox::shouldSend(
    bool connected,
    bool registered,
    uint32_t nowMs
) const
{
    if (empty() || !connected || !registered) return false;
    return _sendDue ||
        (_headHasBeenSent &&
         nowMs - _lastSentAtMs >= _acknowledgementTimeoutMs);
}

bool MeasurementOutbox::isRetry() const { return _headHasBeenSent; }

void MeasurementOutbox::recordSuccessfulSend(uint32_t nowMs)
{
    if (empty()) return;
    _lastSentAtMs = nowMs;
    _headHasBeenSent = true;
    _sendDue = false;
}

void MeasurementOutbox::onTransportUnavailable()
{
    if (!empty() && _headHasBeenSent) _sendDue = true;
}

bool MeasurementOutbox::acknowledge(uint32_t sequence)
{
    if (empty() || front().sequence != sequence) return false;

    _head = (_head + 1) % capacity();
    --_count;
    _headHasBeenSent = false;
    _sendDue = !empty();
    return true;
}

uint32_t MeasurementOutbox::droppedCount() const { return _droppedCount; }
MeasurementType MeasurementOutbox::lastDroppedType() const { return _lastDroppedType; }
uint32_t MeasurementOutbox::lastDroppedSequence() const { return _lastDroppedSequence; }
