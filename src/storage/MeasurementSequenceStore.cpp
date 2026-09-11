#include "storage/MeasurementSequenceStore.h"

#include <Arduino.h>

#include "config/Config.h"

MeasurementSequenceStore::MeasurementSequenceStore(FlashStorage& storage)
    : _storage(storage)
{
}

bool MeasurementSequenceStore::begin()
{
    _initialized = true;
    Serial.println("MeasurementSequenceStore: ready.");
    return true;
}

MeasurementSequenceLoadResult MeasurementSequenceStore::load(
    uint32_t& nextBlockStart
) const
{
    if (!_initialized) return MeasurementSequenceLoadResult::INVALID;
    if (!_storage.exists(STORAGE_KEY)) {
        return MeasurementSequenceLoadResult::NOT_FOUND;
    }

    StoredSequenceState stored = {};
    if (!_storage.getBytes(STORAGE_KEY, &stored, sizeof(stored)) ||
        stored.magic != STORAGE_MAGIC ||
        stored.version != STORAGE_VERSION ||
        !isValidNextBlockStart(stored.nextBlockStart)) {
        Serial.println("MeasurementSequenceStore: invalid persistent state.");
        return MeasurementSequenceLoadResult::INVALID;
    }

    nextBlockStart = stored.nextBlockStart;
    return MeasurementSequenceLoadResult::VALID;
}

bool MeasurementSequenceStore::save(uint32_t nextBlockStart)
{
    if (!_initialized || !isValidNextBlockStart(nextBlockStart)) return false;

    StoredSequenceState stored = {};
    stored.nextBlockStart = nextBlockStart;
    return _storage.setBytes(STORAGE_KEY, &stored, sizeof(stored));
}

bool MeasurementSequenceStore::isValidNextBlockStart(
    uint32_t nextBlockStart
) const
{
    return nextBlockStart >= MEASUREMENT_SEQUENCE_INITIAL_START &&
        nextBlockStart % MEASUREMENT_SEQUENCE_BLOCK_SIZE == 0;
}
