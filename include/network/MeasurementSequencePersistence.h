#pragma once

#include <stdint.h>

enum class MeasurementSequenceLoadResult : uint8_t
{
    NOT_FOUND,
    VALID,
    INVALID
};

class MeasurementSequencePersistence
{
public:
    virtual ~MeasurementSequencePersistence() = default;
    virtual MeasurementSequenceLoadResult load(uint32_t& nextBlockStart) const = 0;
    virtual bool save(uint32_t nextBlockStart) = 0;
};
