#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Application persistence backed by the ESP32 Preferences API (NVS).
class FlashStorage
{
public:
    bool begin();
    bool setString(const char* key, const String& value);
    bool getString(const char* key, String& value);
    bool setBytes(const char* key, const void* data, size_t size);
    bool getBytes(const char* key, void* data, size_t size);
    bool remove(const char* key);
    bool exists(const char* key);
    bool clearAll();
    bool factoryReset();
    bool resetForMaintenance();
    bool clearWifi();
    void debugPrintAll();

private:
    String nvsKey(const char* key) const;
    Preferences _preferences;
    bool _ready = false;
};
