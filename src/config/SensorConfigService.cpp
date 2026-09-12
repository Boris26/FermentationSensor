#include "config/SensorConfigService.h"
#include <Arduino.h>

bool SensorConfigService::begin()
{
    SensorConfig loaded;
    bool found = false;
    if (!_store.load(loaded, found)) {
        Serial.println(found ? "SENSOR_CONFIG_INVALID" : "SENSOR_CONFIG_DEFAULTS");
        _config = SensorConfig::defaults();
    } else if (found) {
        _config = loaded;
        Serial.println("SENSOR_CONFIG_LOADED");
    } else {
        _config = SensorConfig::defaults();
        Serial.println("SENSOR_CONFIG_DEFAULTS");
    }
    _target.applySensorConfig(_config);
    return true;
}

SensorConfigUpdateResult SensorConfigService::update(const SensorConfig& config, const char*& error)
{
    if (!config.validate(error)) { Serial.println("SENSOR_CONFIG_INVALID"); return SensorConfigUpdateResult::INVALID; }
    if (config == _config) { Serial.println("SENSOR_CONFIG_UNCHANGED"); return SensorConfigUpdateResult::UNCHANGED; }
    if (!_store.saveAndVerify(config)) return SensorConfigUpdateResult::STORAGE_ERROR;
    _config = config;
    _target.applySensorConfig(_config);
    Serial.println("SENSOR_CONFIG_SAVED");
    return SensorConfigUpdateResult::CHANGED;
}
