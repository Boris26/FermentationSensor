#include "storage/SensorConfigStore.h"
#include <cstring>

namespace {
constexpr char KEY[] = "sensor_config_v1";
constexpr uint32_t MAGIC = 0x53434647UL; // SCFG
struct Record { uint32_t magic; uint16_t version; uint16_t size; SensorConfig config; };
}

bool SensorConfigStore::load(SensorConfig& config, bool& found)
{
    found = _storage.exists(KEY);
    if (!found) return true;
    Record record = {};
    if (!_storage.getBytes(KEY, &record, sizeof(record))) return false;
    const char* error = nullptr;
    if (record.magic != MAGIC || record.version != SensorConfig::VERSION ||
        record.size != sizeof(SensorConfig) || !record.config.validate(error)) return false;
    config = record.config;
    return true;
}

bool SensorConfigStore::saveAndVerify(const SensorConfig& config)
{
    Record written = {};
    written.magic = MAGIC; written.version = SensorConfig::VERSION;
    written.size = sizeof(SensorConfig); written.config = config;
    if (!_storage.setBytes(KEY, &written, sizeof(written))) return false;
    Record read = {};
    return _storage.getBytes(KEY, &read, sizeof(read)) &&
        std::memcmp(&written, &read, sizeof(written)) == 0;
}
