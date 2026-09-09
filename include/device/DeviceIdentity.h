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
    bool loadDeviceId();
    bool loadDeviceName();

    bool saveDeviceId();
    bool saveDeviceName();

    String generateUuid() const;

    String generateDefaultName(
        const String& uuid
    ) const;

    FlashStorage& _storage;

    String _deviceId;
    String _deviceName;

    bool _initialized = false;
};