#include "storage/FlashStorage.h"

#include <cstring>

namespace
{
constexpr char NVS_NAMESPACE[] = "fermsensor";
constexpr const char* APPLICATION_KEYS[] = {
    "device_config", "wifi_config", "temperature_config", "gateway_cache",
    "measurement_sequence_v1", "sensor_config_v1"
};
}

String FlashStorage::nvsKey(const char* key) const
{
    if (!key) return String();
    if (strcmp(key, "temperature_config") == 0) return "temp_config_v1";
    if (strcmp(key, "measurement_sequence_v1") == 0) return "measure_seq_v1";
    if (strcmp(key, "sensor_config_v1") == 0) return "sensor_cfg_v1";
    // NVS accepts at most 15 characters. Short application keys remain
    // readable in NVS; unknown oversized keys are rejected without collision.
    return strlen(key) <= 15 ? String(key) : String();
}

bool FlashStorage::begin()
{
    if (_ready) return true;
    _ready = _preferences.begin(NVS_NAMESPACE, false);
    Serial.println(_ready ? "FlashStorage: ESP32 NVS ready."
                          : "FlashStorage: unable to open ESP32 NVS.");
    return _ready;
}

bool FlashStorage::setString(const char* key, const String& value)
{
    if (!_ready || !key) return false;
    const String storedKey = nvsKey(key);
    if (storedKey.isEmpty()) return false;
    return _preferences.putString(storedKey.c_str(), value) == value.length();
}

bool FlashStorage::getString(const char* key, String& value)
{
    if (!_ready || !key || !exists(key)) return false;
    value = _preferences.getString(nvsKey(key).c_str(), "");
    return true;
}

bool FlashStorage::setBytes(const char* key, const void* data, size_t size)
{
    if (!_ready || !key || !data || size == 0) return false;
    const String storedKey = nvsKey(key);
    if (storedKey.isEmpty()) return false;
    return _preferences.putBytes(storedKey.c_str(), data, size) == size;
}

bool FlashStorage::getBytes(const char* key, void* data, size_t size)
{
    if (!_ready || !key || !data || size == 0) return false;
    const String storedKey = nvsKey(key);
    if (storedKey.isEmpty()) return false;
    if (_preferences.getBytesLength(storedKey.c_str()) != size) return false;
    return _preferences.getBytes(storedKey.c_str(), data, size) == size;
}

bool FlashStorage::remove(const char* key)
{
    if (!_ready || !key) return false;
    const String storedKey = nvsKey(key);
    return !storedKey.isEmpty() && (!exists(key) || _preferences.remove(storedKey.c_str()));
}

bool FlashStorage::exists(const char* key)
{
    if (!_ready || !key) return false;
    const String storedKey = nvsKey(key);
    return !storedKey.isEmpty() && _preferences.isKey(storedKey.c_str());
}

bool FlashStorage::clearAll() { return factoryReset(); }

bool FlashStorage::factoryReset()
{
    if (!_ready) return false;
    Serial.println("FlashStorage: clearing ESP32 NVS namespace...");
    return _preferences.clear();
}

bool FlashStorage::resetForMaintenance()
{
    return _ready && _preferences.clear();
}

bool FlashStorage::clearWifi()
{
    return !exists("wifi_config") || remove("wifi_config");
}

void FlashStorage::debugPrintAll()
{
    Serial.println("========== ESP32 NVS ==========");
    for (const char* key : APPLICATION_KEYS) {
        if (!exists(key)) continue;
        Serial.print(key);
        Serial.print(",bytes=");
        Serial.println(_preferences.getBytesLength(nvsKey(key).c_str()));
    }
    Serial.println("================================");
}
