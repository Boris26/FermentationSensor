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
    static constexpr size_t DEVICE_ID_LENGTH = 36;
    static constexpr size_t MAX_DEVICE_NAME_LENGTH = 63;

    struct StoredDeviceConfiguration
    {
        char deviceId[DEVICE_ID_LENGTH + 1] = {};
        char deviceName[MAX_DEVICE_NAME_LENGTH + 1] = {};
    };

    bool loadConfiguration();
    bool saveConfiguration(
        const String& deviceId,
        const String& deviceName
    );

    bool isValidDeviceId(
        const String& deviceId
    ) const;

    bool isValidDeviceName(
        const String& name
    ) const;

    String generateUuid() const;

    String generateDefaultName(
        const String& uuid
    ) const;

    FlashStorage& _storage;

    String _deviceId;
    String _deviceName;

    bool _initialized = false;
};
