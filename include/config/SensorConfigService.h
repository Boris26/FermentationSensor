#pragma once

#include "config/SensorConfig.h"
#include "storage/SensorConfigStore.h"

class SensorConfigTarget {
public:
    virtual ~SensorConfigTarget() = default;
    virtual void applySensorConfig(const SensorConfig& config) = 0;
};

enum class SensorConfigUpdateResult { CHANGED, UNCHANGED, INVALID, STORAGE_ERROR };

class SensorConfigService {
public:
    SensorConfigService(SensorConfigStore& store, SensorConfigTarget& target)
        : _store(store), _target(target) {}
    bool begin();
    const SensorConfig& get() const { return _config; }
    SensorConfigUpdateResult update(const SensorConfig& config, const char*& error);
private:
    SensorConfigStore& _store;
    SensorConfigTarget& _target;
    SensorConfig _config = SensorConfig::defaults();
};
