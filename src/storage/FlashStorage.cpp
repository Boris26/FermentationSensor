#include "storage/FlashStorage.h"

namespace
{
constexpr char NVS_NAMESPACE[] = "fermsensor";
constexpr const char* APPLICATION_KEYS[] = {
    "device_config", "wifi_config", "temperature_config", "gateway_cache",
    "measurement_sequence_v1", "sensor_config_v1", "wifi_ssid",
    "wifi_password", "device_uuid", "device_name", "server_config"
};
}

String FlashStorage::nvsKey(const char* key) const
{
    // Preferences keys are limited to 15 characters. A stable FNV-1a-derived
    // key keeps the public storage interface and its descriptive keys intact.
    uint32_t hash = 2166136261UL;
    for (const uint8_t* p = reinterpret_cast<const uint8_t*>(key); *p; ++p) {
        hash ^= *p;
        hash *= 16777619UL;
    }
    char encoded[10] = {};
    snprintf(encoded, sizeof(encoded), "k%08lx", static_cast<unsigned long>(hash));
    return String(encoded);
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
    return _preferences.putBytes(nvsKey(key).c_str(), data, size) == size;
}

bool FlashStorage::getBytes(const char* key, void* data, size_t size)
{
    if (!_ready || !key || !data || size == 0) return false;
    const String storedKey = nvsKey(key);
    if (_preferences.getBytesLength(storedKey.c_str()) != size) return false;
    return _preferences.getBytes(storedKey.c_str(), data, size) == size;
}

bool FlashStorage::remove(const char* key)
{
    if (!_ready || !key) return false;
    return !exists(key) || _preferences.remove(nvsKey(key).c_str());
}

bool FlashStorage::exists(const char* key)
{
    return _ready && key && _preferences.isKey(nvsKey(key).c_str());
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
    for (const char* key : {"wifi_config", "wifi_ssid", "wifi_password"}) {
        if (exists(key) && !remove(key)) return false;
    }
    return true;
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
