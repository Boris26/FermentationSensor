#include "storage/FlashStorage.h"

#include <Arduino.h>
#include <kvstore_global_api.h>

namespace
{
constexpr int KV_SUCCESS = 0;

constexpr size_t STRING_BUFFER_SIZE = 128;
}

bool FlashStorage::begin()
{
    Serial.println("FlashStorage: ready.");
    return true;
}

bool FlashStorage::setString(
    const char* key,
    const String& value
)
{
    const int result =
        kv_set(
            key,
            value.c_str(),
            value.length() + 1,
            0
        );

    if (result != KV_SUCCESS) {
        Serial.print(
            "FlashStorage: failed to write key: "
        );
        Serial.println(key);

        return false;
    }

    return true;
}

bool FlashStorage::getString(
    const char* key,
    String& value
)
{
    char buffer[STRING_BUFFER_SIZE] = {};

    size_t actualSize = 0;

    const int result =
        kv_get(
            key,
            buffer,
            sizeof(buffer) - 1,
            &actualSize
        );

    if (result != KV_SUCCESS) {
        return false;
    }

    if (actualSize == 0) {
        value = "";
        return true;
    }

    if (actualSize >= sizeof(buffer)) {
        Serial.print(
            "FlashStorage: stored string too large for key: "
        );
        Serial.println(key);

        return false;
    }

    buffer[actualSize] = '\0';

    value = String(buffer);

    return true;
}

bool FlashStorage::setBytes(
    const char* key,
    const void* data,
    size_t size
)
{
    if (data == nullptr || size == 0) {
        return false;
    }

    const int result =
        kv_set(
            key,
            data,
            size,
            0
        );

    if (result != KV_SUCCESS) {
        Serial.print(
            "FlashStorage: failed to write key: "
        );
        Serial.println(key);

        return false;
    }

    return true;
}

bool FlashStorage::getBytes(
    const char* key,
    void* data,
    size_t size
)
{
    if (data == nullptr || size == 0) {
        return false;
    }

    size_t actualSize = 0;

    const int result =
        kv_get(
            key,
            data,
            size,
            &actualSize
        );

    if (result != KV_SUCCESS) {
        return false;
    }

    if (actualSize != size) {
        Serial.print(
            "FlashStorage: unexpected data size for key: "
        );
        Serial.println(key);

        return false;
    }

    return true;
}

bool FlashStorage::remove(
    const char* key
)
{
    const int result =
        kv_remove(key);

    if (result != KV_SUCCESS) {
        Serial.print(
            "FlashStorage: failed to remove key: "
        );
        Serial.println(key);

        return false;
    }

    return true;
}

bool FlashStorage::exists(
    const char* key
)
{
    char dummy = 0;
    size_t actualSize = 0;

    const int result =
        kv_get(
            key,
            &dummy,
            sizeof(dummy),
            &actualSize
        );

    return result == KV_SUCCESS;
}