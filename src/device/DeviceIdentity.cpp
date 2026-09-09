#include "device/DeviceIdentity.h"

#include <Arduino.h>
#include <cstring>

namespace
{
constexpr char DEVICE_CONFIG_KEY[] =
    "device_config";
}


DeviceIdentity::DeviceIdentity(
    FlashStorage& storage
)
    : _storage(storage)
{
}


bool DeviceIdentity::begin()
{
    Serial.println(
        "DeviceIdentity: initializing..."
    );


    if (!loadConfiguration()) {

        _deviceId =
            generateUuid();

        _deviceName =
            generateDefaultName(
                _deviceId
            );


        if (!saveConfiguration()) {
            Serial.println(
                "DeviceIdentity: failed to save configuration."
            );

            return false;
        }


        Serial.print(
            "DeviceIdentity: generated UUID: "
        );

        Serial.println(
            _deviceId
        );


        Serial.print(
            "DeviceIdentity: generated device name: "
        );

        Serial.println(
            _deviceName
        );
    }


    _initialized = true;


    Serial.print(
        "DeviceIdentity: ID: "
    );

    Serial.println(
        _deviceId
    );


    Serial.print(
        "DeviceIdentity: name: "
    );

    Serial.println(
        _deviceName
    );


    return true;
}


const String& DeviceIdentity::getDeviceId() const
{
    return _deviceId;
}


const String& DeviceIdentity::getDeviceName() const
{
    return _deviceName;
}


bool DeviceIdentity::setDeviceName(
    const String& name
)
{
    if (!_initialized) {
        return false;
    }


    String trimmedName =
        name;

    trimmedName.trim();


    if (trimmedName.isEmpty()) {
        Serial.println(
            "DeviceIdentity: invalid device name."
        );

        return false;
    }


    _deviceName =
        trimmedName;


    if (!saveConfiguration()) {
        Serial.println(
            "DeviceIdentity: failed to save device name."
        );

        return false;
    }


    Serial.print(
        "DeviceIdentity: device name changed to "
    );

    Serial.println(
        _deviceName
    );


    return true;
}


bool DeviceIdentity::loadConfiguration()
{
    StoredDeviceConfiguration stored = {};


    if (
        !_storage.getBytes(
            DEVICE_CONFIG_KEY,
            &stored,
            sizeof(stored)
        )
    ) {
        return false;
    }


    if (
        stored.deviceId[0] == '\0' ||
        stored.deviceName[0] == '\0'
    ) {
        return false;
    }


    _deviceId =
        String(stored.deviceId);

    _deviceName =
        String(stored.deviceName);


    return true;
}


bool DeviceIdentity::saveConfiguration()
{
    if (
        _deviceId.isEmpty() ||
        _deviceName.isEmpty()
    ) {
        return false;
    }


    StoredDeviceConfiguration stored = {};


    _deviceId.toCharArray(
        stored.deviceId,
        sizeof(stored.deviceId)
    );


    _deviceName.toCharArray(
        stored.deviceName,
        sizeof(stored.deviceName)
    );


    return _storage.setBytes(
        DEVICE_CONFIG_KEY,
        &stored,
        sizeof(stored)
    );
}


String DeviceIdentity::generateUuid() const
{
    char uuid[37];


    const uint32_t part1 =
        static_cast<uint32_t>(
            random(0x7FFFFFFF)
        );


    const uint16_t part2 =
        static_cast<uint16_t>(
            random(0xFFFF)
        );


    // UUID version 4
    const uint16_t part3 =
        static_cast<uint16_t>(
            random(0x0FFF)
        ) | 0x4000;


    // UUID variant 1
    const uint16_t part4 =
        static_cast<uint16_t>(
            random(0x3FFF)
        ) | 0x8000;


    const uint16_t part5a =
        static_cast<uint16_t>(
            random(0xFFFF)
        );


    const uint32_t part5b =
        static_cast<uint32_t>(
            random(0x7FFFFFFF)
        );


    snprintf(
        uuid,
        sizeof(uuid),
        "%08lx-%04x-%04x-%04x-%04x%08lx",
        static_cast<unsigned long>(part1),
        part2,
        part3,
        part4,
        part5a,
        static_cast<unsigned long>(part5b)
    );


    return String(uuid);
}


String DeviceIdentity::generateDefaultName(
    const String& uuid
) const
{
    String suffix;


    for (
        int index =
            uuid.length() - 1;

        index >= 0 &&
        suffix.length() < 6;

        --index
    ) {
        const char character =
            uuid[index];


        if (character == '-') {
            continue;
        }


        suffix =
            String(character) +
            suffix;
    }


    suffix.toUpperCase();


    return
        String("FERM-") +
        suffix;
}