#include "network/MeasurementSequenceAllocator.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

MeasurementSequenceAllocator::MeasurementSequenceAllocator(
    MeasurementSequencePersistence& store
) : _store(store)
{
}

bool MeasurementSequenceAllocator::begin()
{
    uint32_t blockStart = 0;
    const MeasurementSequenceLoadResult result = _store.load(blockStart);
    if (result == MeasurementSequenceLoadResult::INVALID) {
        printReservationFailure();
        return false;
    }
    if (result == MeasurementSequenceLoadResult::NOT_FOUND) {
        blockStart = MEASUREMENT_SEQUENCE_INITIAL_START;
    }
    if (!reserveBlock(blockStart)) {
        printReservationFailure();
        return false;
    }
    return true;
}

bool MeasurementSequenceAllocator::next(uint32_t& sequence)
{
    if (!_hasReservedBlock) {
        printReservationFailure();
        return false;
    }

    if (_nextSequence > _blockEnd) {
        if (_blockEnd == UINT32_MAX || !reserveBlock(_blockEnd + 1U)) {
            printReservationFailure();
            return false;
        }
    }

    sequence = _nextSequence;
    ++_nextSequence;
    return true;
}

bool MeasurementSequenceAllocator::reserveBlock(uint32_t blockStart)
{
    // A complete block and its following free-block marker must both fit in
    // uint32_t. Consequently the final 0xFFFF0000 block is never exposed.
    if (blockStart > UINT32_MAX - (2U * MEASUREMENT_SEQUENCE_BLOCK_SIZE - 1U)) {
        _hasReservedBlock = false;
        return false;
    }

    const uint32_t persistedNext = blockStart + MEASUREMENT_SEQUENCE_BLOCK_SIZE;
    if (!_store.save(persistedNext)) {
        _hasReservedBlock = false;
        return false;
    }

    _nextSequence = blockStart;
    _blockEnd = persistedNext - 1U;
    _hasReservedBlock = true;
    _failureReported = false;
#ifdef ARDUINO
    Serial.print("MEASUREMENT_SEQUENCE_BLOCK,");
    Serial.print(blockStart);
    Serial.print(',');
    Serial.println(_blockEnd);
#endif
    return true;
}

void MeasurementSequenceAllocator::printReservationFailure()
{
    if (_failureReported) return;
    _failureReported = true;
#ifdef ARDUINO
    Serial.println("MEASUREMENT_SEQUENCE_RESERVATION_FAILED");
#endif
}
