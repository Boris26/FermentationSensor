#pragma once

#include <stdint.h>

#include "network/MeasurementSequencePersistence.h"
#include "storage/FlashStorage.h"

class MeasurementSequenceStore : public MeasurementSequencePersistence
{
public:
    explicit MeasurementSequenceStore(FlashStorage& storage);

    bool begin();
    MeasurementSequenceLoadResult load(uint32_t& nextBlockStart) const override;
    bool save(uint32_t nextBlockStart) override;

private:
    static constexpr uint32_t STORAGE_MAGIC = 0x4D535131UL; // "MSQ1"
    static constexpr uint8_t STORAGE_VERSION = 1;

    struct StoredSequenceState
    {
        uint32_t magic = STORAGE_MAGIC;
        uint32_t nextBlockStart = 0;
        uint8_t version = STORAGE_VERSION;
        uint8_t reserved[3] = {};
    };

    static constexpr const char* STORAGE_KEY = "measurement_sequence_v1";

    bool isValidNextBlockStart(uint32_t nextBlockStart) const;

    FlashStorage& _storage;
    bool _initialized = false;
};
