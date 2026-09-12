#include "device/DeviceIdentity.h"

#include <Arduino.h>
#include <cstring>
#include <WiFiNINA.h>

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


        if (
            !saveConfiguration(
                _deviceId,
                _deviceName
            )
        ) {
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
    return updateDeviceName(name) == NameUpdateResult::CHANGED;
}

DeviceIdentity::NameUpdateResult DeviceIdentity::updateDeviceName(const String& name)
{
    if (!_initialized) {
        return NameUpdateResult::STORAGE_ERROR;
    }


    String trimmedName =
        name;

    trimmedName.trim();


    if (!isValidDeviceName(trimmedName)) {
        Serial.println(
            "DeviceIdentity: invalid device name."
        );

        return NameUpdateResult::INVALID;
    }

    if (trimmedName == _deviceName) return NameUpdateResult::UNCHANGED;


    if (
        !saveConfiguration(
            _deviceId,
            trimmedName
        )
    ) {
        Serial.println(
            "DeviceIdentity: failed to save device name."
        );

        return NameUpdateResult::STORAGE_ERROR;
    }

    StoredDeviceConfiguration verified = {};
    if (!_storage.getBytes(DEVICE_CONFIG_KEY, &verified, sizeof(verified)) ||
        String(verified.deviceId) != _deviceId || String(verified.deviceName) != trimmedName) {
        // Runtime identity remains unchanged if durable verification fails.
        return NameUpdateResult::STORAGE_ERROR;
    }


    _deviceName =
        trimmedName;


    Serial.print(
        "DeviceIdentity: device name changed to "
    );

    Serial.println(
        _deviceName
    );


    return NameUpdateResult::CHANGED;
}


bool DeviceIdentity::loadConfiguration()
{
    StoredDeviceConfiguration stored = {};


    if (!_storage.exists(DEVICE_CONFIG_KEY)) {
        return false;
    }


    if (
        !_storage.getBytes(
            DEVICE_CONFIG_KEY,
            &stored,
            sizeof(stored)
        )
    ) {
        Serial.println(
            "DeviceIdentity: failed to load stored configuration."
        );

        return false;
    }


    if (
        stored.deviceId[0] == '\0' ||
        stored.deviceId[
            sizeof(stored.deviceId) - 1
        ] != '\0'
    ) {
        Serial.println(
            "DeviceIdentity: stored device ID is invalid."
        );

        return false;
    }


    _deviceId =
        String(stored.deviceId);

    if (!isValidDeviceId(_deviceId)) {
        Serial.println(
            "DeviceIdentity: stored device ID has an invalid format."
        );

        return false;
    }

    if (
        stored.deviceName[
            sizeof(stored.deviceName) - 1
        ] == '\0' &&
        isValidDeviceName(
            String(stored.deviceName)
        )
    ) {
        _deviceName =
            String(stored.deviceName);
    }
    else {
        Serial.println(
            "DeviceIdentity: stored device name is invalid; using default."
        );

        _deviceName =
            generateDefaultName(
                _deviceId
            );

        if (
            !saveConfiguration(
                _deviceId,
                _deviceName
            )
        ) {
            return false;
        }
    }


    return true;
}


bool DeviceIdentity::isValidDeviceId(
    const String& deviceId
) const
{
    if (deviceId.length() != DEVICE_ID_LENGTH) {
        return false;
    }

    for (size_t index = 0; index < DEVICE_ID_LENGTH; ++index) {
        const char character =
            deviceId[index];

        const bool separator =
            index == 8 ||
            index == 13 ||
            index == 18 ||
            index == 23;

        if (separator) {
            if (character != '-') {
                return false;
            }

            continue;
        }

        const bool hexadecimal =
            (
                character >= '0' &&
                character <= '9'
            ) ||
            (
                character >= 'a' &&
                character <= 'f'
            ) ||
            (
                character >= 'A' &&
                character <= 'F'
            );

        if (!hexadecimal) {
            return false;
        }
    }

    return
        deviceId[14] == '4' &&
        (
            deviceId[19] == '8' ||
            deviceId[19] == '9' ||
            deviceId[19] == 'a' ||
            deviceId[19] == 'A' ||
            deviceId[19] == 'b' ||
            deviceId[19] == 'B'
        );
}


bool DeviceIdentity::isValidDeviceName(
    const String& name
) const
{
    if (
        name.isEmpty() ||
        name.length() > MAX_DEVICE_NAME_LENGTH
    ) {
        return false;
    }

    for (size_t index = 0; index < name.length(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(name[index]);

        const bool isLetter =
            (
                character >= 'A' &&
                character <= 'Z'
            ) ||
            (
                character >= 'a' &&
                character <= 'z'
            );

        const bool isNumber =
            character >= '0' &&
            character <= '9';

        const bool allowed =
            isLetter ||
            isNumber ||
            character == ' ' ||
            character == '-' ||
            character == '_' ||
            character == '.' ||
            character == '(' ||
            character == ')' ||
            character >= 0x80;

        if (!allowed) {
            return false;
        }
    }

    return true;
}


bool DeviceIdentity::saveConfiguration(
    const String& deviceId,
    const String& deviceName
)
{
    if (
        !isValidDeviceId(deviceId) ||
        !isValidDeviceName(deviceName)
    ) {
        Serial.println(
            "DeviceIdentity: refusing to save invalid configuration."
        );

        return false;
    }


    StoredDeviceConfiguration stored = {};


    deviceId.toCharArray(
        stored.deviceId,
        sizeof(stored.deviceId)
    );


    deviceName.toCharArray(
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

    byte mac[6] = {};
    WiFi.macAddress(mac);

    uint32_t seed = 2166136261UL;
    for (size_t index = 0; index < sizeof(mac); ++index) {
        seed ^= mac[index];
        seed *= 16777619UL;
    }
    seed ^= micros();
    seed ^= static_cast<uint32_t>(analogRead(A0)) << 16;
    randomSeed(seed);


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
