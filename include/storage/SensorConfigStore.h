#pragma once

#include "config/SensorConfig.h"
#include "storage/FlashStorage.h"

class SensorConfigStore {
public:
    explicit SensorConfigStore(FlashStorage& storage) : _storage(storage) {}
    bool begin() { return true; }
    bool load(SensorConfig& config, bool& found);
    bool saveAndVerify(const SensorConfig& config);
private:
    FlashStorage& _storage;
};
