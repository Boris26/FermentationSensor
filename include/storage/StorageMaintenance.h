#pragma once

#include "storage/FlashStorage.h"

// Explicit TDBStore compaction. This is intentionally separate from both
// runtime measurement-state reset and factory reset.
class StorageMaintenance
{
public:
    explicit StorageMaintenance(FlashStorage& storage);

    // Must be called only while normal measurement processing is stopped.
    // On success, callers must initialize the MeasurementSequenceAllocator
    // normally; it will durably reserve the backed-up next-safe block.
    bool compactPersistentStorage();

private:
    FlashStorage& _storage;
};
