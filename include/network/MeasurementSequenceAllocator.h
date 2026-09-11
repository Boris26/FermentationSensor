#pragma once

#include <stdint.h>

#include "config/Config.h"
#include "network/MeasurementSequencePersistence.h"

class MeasurementSequenceSource
{
public:
    virtual ~MeasurementSequenceSource() = default;
    virtual bool next(uint32_t& sequence) = 0;
};

// Reserves whole ranges durably before exposing any value from them. Unused
// values are deliberately abandoned on reboot so no possibly-used sequence can
// be issued again for the stable device identity.
class MeasurementSequenceAllocator : public MeasurementSequenceSource
{
public:
    explicit MeasurementSequenceAllocator(MeasurementSequencePersistence& store);

    bool begin();
    bool next(uint32_t& sequence) override;

private:
    bool reserveBlock(uint32_t blockStart);
    void printReservationFailure();

    MeasurementSequencePersistence& _store;
    uint32_t _nextSequence = 0;
    uint32_t _blockEnd = 0;
    bool _hasReservedBlock = false;
    bool _failureReported = false;
};
