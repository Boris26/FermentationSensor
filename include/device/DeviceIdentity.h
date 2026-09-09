#pragma once

#include <Arduino.h>

#include "storage/FlashStorage.h"

class DeviceIdentity
{
public:
    explicit DeviceIdentity(
        FlashStorage& storage
    );

    bool begin();

    const String& getDeviceId() const;
    const String& getDeviceName() const;

    bool setDeviceName(
        const String& name
    );

private:
    struct StoredDeviceConfiguration
    {
        char deviceId[37] = {};
        char deviceName[64] = {};
    };

    bool loadConfiguration();
    bool saveConfiguration();

    String generateUuid() const;

    String generateDefaultName(
        const String& uuid
    ) const;

    FlashStorage& _storage;

    String _deviceId;
    String _deviceName;

    bool _initialized = false;
};