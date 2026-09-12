#include "storage/StorageMaintenance.h"

#include <Arduino.h>
#include <cstring>

#include "config/Config.h"
#include "config/SensorConfig.h"

namespace
{
constexpr char DEVICE_KEY[] = "device_config";
constexpr char WIFI_KEY[] = "wifi_config";
constexpr char TEMPERATURE_KEY[] = "temperature_config";
constexpr char GATEWAY_KEY[] = "gateway_cache";
constexpr char SEQUENCE_KEY[] = "measurement_sequence_v1";
constexpr char SENSOR_CONFIG_KEY[] = "sensor_config_v1";

struct DeviceRecord { char id[37]; char name[64]; };
struct WifiRecord { uint8_t version; char ssid[33]; char password[64]; };
struct TemperatureRecord {
    uint8_t hasAmbient;
    uint8_t hasBeer;
    uint8_t ambient[8];
    uint8_t beer[8];
};
struct GatewayRecord {
    char address[16];
    uint16_t port;
    char path[96];
    uint16_t protocolVersion;
};
struct SequenceRecord {
    uint32_t magic;
    uint32_t nextBlockStart;
    uint8_t version;
    uint8_t reserved[3];
};
struct SensorConfigurationRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    SensorConfig config;
};

static_assert(sizeof(DeviceRecord) == 101, "device record layout changed");
static_assert(sizeof(WifiRecord) == 98, "wifi record layout changed");
static_assert(sizeof(TemperatureRecord) == 18, "temperature record layout changed");
static_assert(sizeof(GatewayRecord) == 116, "gateway record layout changed");
static_assert(sizeof(SequenceRecord) == 12, "sequence record layout changed");

template<typename T> struct Backup {
    bool present = false;
    T value = {};
};

bool hasNonzeroId(const uint8_t* id)
{
    for (size_t i = 0; i < 8; ++i) if (id[i] != 0) return true;
    return false;
}

bool validUuid(const char* value)
{
    if (value[36] != '\0' || value[14] != '4') return false;
    for (size_t i = 0; i < 36; ++i) {
        const bool separator = i == 8 || i == 13 || i == 18 || i == 23;
        if (separator) { if (value[i] != '-') return false; continue; }
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return false;
    }
    const char variant = value[19];
    return variant == '8' || variant == '9' || variant == 'a' ||
        variant == 'A' || variant == 'b' || variant == 'B';
}

bool validName(const char* value)
{
    if (value[0] == '\0' || value[63] != '\0') return false;
    for (size_t i = 0; value[i] != '\0'; ++i) {
        const char c = value[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_' ||
              c == '.' || c == '(' || c == ')' || c >= 0x80)) return false;
    }
    return true;
}

bool valid(const DeviceRecord& r) { return validUuid(r.id) && validName(r.name); }
bool valid(const WifiRecord& r)
{
    return r.version == 1 && r.ssid[0] != '\0' && r.ssid[32] == '\0' &&
        r.password[63] == '\0';
}
bool valid(const TemperatureRecord& r)
{
    return r.hasAmbient <= 1 && r.hasBeer <= 1 &&
        (!r.hasAmbient || hasNonzeroId(r.ambient)) &&
        (!r.hasBeer || hasNonzeroId(r.beer));
}
bool valid(const GatewayRecord& r)
{
    return r.address[0] != '\0' && r.address[15] == '\0' && r.port != 0 &&
        r.path[0] == '/' && r.path[95] == '\0' && r.protocolVersion == 1;
}
bool valid(const SequenceRecord& r)
{
    return r.magic == 0x4D535131UL && r.version == 1 &&
        r.nextBlockStart >= MEASUREMENT_SEQUENCE_INITIAL_START &&
        r.nextBlockStart % MEASUREMENT_SEQUENCE_BLOCK_SIZE == 0 &&
        r.nextBlockStart <= UINT32_MAX -
            (2U * MEASUREMENT_SEQUENCE_BLOCK_SIZE - 1U);
}
bool valid(const SensorConfigurationRecord& r)
{
    const char* error = nullptr;
    return r.magic == 0x53434647UL && r.version == SensorConfig::VERSION &&
        r.size == sizeof(SensorConfig) && r.config.validate(error);
}

template<typename T>
bool backupRecord(FlashStorage& storage, const char* key, bool required, Backup<T>& out)
{
    out.present = storage.exists(key);
    if (!out.present) return !required;
    return storage.getBytes(key, &out.value, sizeof(out.value)) && valid(out.value);
}

template<typename T>
bool restoreRecord(FlashStorage& storage, const char* key, const Backup<T>& backup)
{
    return !backup.present || storage.setBytes(key, &backup.value, sizeof(backup.value));
}

template<typename T>
bool verifyRecord(FlashStorage& storage, const char* key, const Backup<T>& backup)
{
    if (!backup.present) return !storage.exists(key);
    T restored = {};
    return storage.getBytes(key, &restored, sizeof(restored)) && valid(restored) &&
        memcmp(&restored, &backup.value, sizeof(restored)) == 0;
}

bool fail(const char* phase)
{
    Serial.print("STORAGE_MAINTENANCE_FAILED,");
    Serial.println(phase);
    return false;
}
}

StorageMaintenance::StorageMaintenance(FlashStorage& storage) : _storage(storage) {}

bool StorageMaintenance::compactPersistentStorage()
{
    Serial.println("STORAGE_MAINTENANCE_START");
    Serial.println("STORAGE_MAINTENANCE_WARNING,POWER_LOSS_CAN_DESTROY_PERSISTENT_STATE");

    Backup<DeviceRecord> device;
    Backup<WifiRecord> wifi;
    Backup<TemperatureRecord> temperature;
    Backup<GatewayRecord> gateway;
    Backup<SequenceRecord> sequence;
    Backup<SensorConfigurationRecord> sensorConfig;
    if (!backupRecord(_storage, DEVICE_KEY, true, device) ||
        !backupRecord(_storage, WIFI_KEY, false, wifi) ||
        !backupRecord(_storage, TEMPERATURE_KEY, false, temperature) ||
        !backupRecord(_storage, GATEWAY_KEY, false, gateway) ||
        !backupRecord(_storage, SEQUENCE_KEY, true, sequence) ||
        !backupRecord(_storage, SENSOR_CONFIG_KEY, false, sensorConfig)) return fail("BACKUP");

    Serial.println("STORAGE_MAINTENANCE_BACKUP_OK");
    if (!_storage.resetForMaintenance()) return fail("RESET");
    Serial.println("STORAGE_MAINTENANCE_RESET_OK");

    if (!restoreRecord(_storage, DEVICE_KEY, device) ||
        !restoreRecord(_storage, WIFI_KEY, wifi) ||
        !restoreRecord(_storage, TEMPERATURE_KEY, temperature) ||
        !restoreRecord(_storage, GATEWAY_KEY, gateway) ||
        !restoreRecord(_storage, SEQUENCE_KEY, sequence) ||
        !restoreRecord(_storage, SENSOR_CONFIG_KEY, sensorConfig)) return fail("RESTORE");
    Serial.println("STORAGE_MAINTENANCE_RESTORE_OK");

    if (!verifyRecord(_storage, DEVICE_KEY, device) ||
        !verifyRecord(_storage, WIFI_KEY, wifi) ||
        !verifyRecord(_storage, TEMPERATURE_KEY, temperature) ||
        !verifyRecord(_storage, GATEWAY_KEY, gateway) ||
        !verifyRecord(_storage, SEQUENCE_KEY, sequence) ||
        !verifyRecord(_storage, SENSOR_CONFIG_KEY, sensorConfig)) return fail("VERIFY");

    Serial.println("STORAGE_MAINTENANCE_SUCCESS");
    return true;
}
