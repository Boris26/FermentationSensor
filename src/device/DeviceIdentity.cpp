#include "device/DeviceIdentity.h"

#include <Arduino.h>

namespace
{
constexpr char DEVICE_ID_KEY[] =
    "device_uuid";

constexpr char DEVICE_NAME_KEY[] =
    "device_name";
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


    if (!loadDeviceId()) {
        _deviceId =
            generateUuid();


        if (!saveDeviceId()) {
            Serial.println(
                "DeviceIdentity: failed to save UUID."
            );

            return false;
        }


        Serial.print(
            "DeviceIdentity: generated UUID: "
        );

        Serial.println(
            _deviceId
        );
    }


    if (!loadDeviceName()) {
        _deviceName =
            generateDefaultName(
                _deviceId
            );


        if (!saveDeviceName()) {
            Serial.println(
                "DeviceIdentity: failed to save device name."
            );

            return false;
        }


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


    if (!saveDeviceName()) {
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


bool DeviceIdentity::loadDeviceId()
{
    return _storage.getString(
        DEVICE_ID_KEY,
        _deviceId
    );
}


bool DeviceIdentity::loadDeviceName()
{
    return _storage.getString(
        DEVICE_NAME_KEY,
        _deviceName
    );
}


bool DeviceIdentity::saveDeviceId()
{
    return _storage.setString(
        DEVICE_ID_KEY,
        _deviceId
    );
}


bool DeviceIdentity::saveDeviceName()
{
    return _storage.setString(
        DEVICE_NAME_KEY,
        _deviceName
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