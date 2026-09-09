#include "storage/FlashStorage.h"

#include <Arduino.h>
#include <kvstore_global_api.h>

namespace
{
constexpr int KV_SUCCESS = 0;

constexpr size_t STRING_BUFFER_SIZE = 128;

constexpr char KV_PREFIX[] = "/kv/";
}


bool FlashStorage::begin()
{
    Serial.println(
        "FlashStorage: ready."
    );

    return true;
}


String FlashStorage::buildKey(
    const char* key
) const
{
    return
        String(KV_PREFIX) +
        key;
}


bool FlashStorage::setString(
    const char* key,
    const String& value
)
{
    const String fullKey =
        buildKey(key);


    const int result =
        kv_set(
            fullKey.c_str(),
            value.c_str(),
            value.length() + 1,
            0
        );


    if (result != KV_SUCCESS) {
        Serial.print(
            "FlashStorage: failed to write key: "
        );

        Serial.print(
            fullKey
        );

        Serial.print(
            " result="
        );

        Serial.println(
            result
        );


        Serial.print(
            "FlashStorage: value length="
        );

        Serial.println(
            value.length()
        );

        return false;
    }


    return true;
}


bool FlashStorage::getString(
    const char* key,
    String& value
)
{
    const String fullKey =
        buildKey(key);


    char buffer[
        STRING_BUFFER_SIZE
    ] = {};


    size_t actualSize = 0;


    const int result =
        kv_get(
            fullKey.c_str(),
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

        Serial.println(
            fullKey
        );

        return false;
    }


    buffer[actualSize] =
        '\0';


    value =
        String(buffer);


    return true;
}


bool FlashStorage::setBytes(
    const char* key,
    const void* data,
    size_t size
)
{
    if (
        data == nullptr ||
        size == 0
    ) {
        return false;
    }


    const String fullKey =
        buildKey(key);


    const int result =
        kv_set(
            fullKey.c_str(),
            data,
            size,
            0
        );


    if (result != KV_SUCCESS) {
        Serial.print(
            "FlashStorage: failed to write key: "
        );

        Serial.print(
            fullKey
        );

        Serial.print(
            " result="
        );

        Serial.println(
            result
        );


        Serial.print(
            "FlashStorage: data size="
        );

        Serial.println(
            size
        );

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
    if (
        data == nullptr ||
        size == 0
    ) {
        return false;
    }


    const String fullKey =
        buildKey(key);


    size_t actualSize = 0;


    const int result =
        kv_get(
            fullKey.c_str(),
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

        Serial.println(
            fullKey
        );

        return false;
    }


    return true;
}


bool FlashStorage::remove(
    const char* key
)
{
    const String fullKey =
        buildKey(key);


    const int result =
        kv_remove(
            fullKey.c_str()
        );


    if (result != KV_SUCCESS) {
        Serial.print(
            "FlashStorage: failed to remove key: "
        );

        Serial.print(
            fullKey
        );

        Serial.print(
            " result="
        );

        Serial.println(
            result
        );

        return false;
    }


    return true;
}


bool FlashStorage::exists(
    const char* key
)
{
    const String fullKey =
        buildKey(key);


    kv_info_t info;


    const int result =
        kv_get_info(
            fullKey.c_str(),
            &info
        );


    return
        result == KV_SUCCESS;
}


bool FlashStorage::clearAll()
{
    Serial.println(
        "FlashStorage: clearing complete KVStore..."
    );


    const int result =
        kv_reset(
            KV_PREFIX
        );


    if (result != KV_SUCCESS) {
        Serial.print(
            "FlashStorage: clear failed result="
        );

        Serial.println(
            result
        );

        return false;
    }


    Serial.println(
        "FlashStorage: KVStore cleared."
    );


    return true;
}


void FlashStorage::debugPrintAll()
{
    Serial.println();

    Serial.println(
        "========== FlashStorage =========="
    );


    String value;


    auto printString =
        [&](const char* key)
        {
            if (
                getString(
                    key,
                    value
                )
            ) {
                Serial.print(
                    key
                );

                Serial.print(
                    ": "
                );

                Serial.println(
                    value
                );
            } else {
                Serial.print(
                    key
                );

                Serial.println(
                    ": <not set>"
                );
            }
        };


    printString(
        "device_uuid"
    );

    printString(
        "device_name"
    );

    printString(
        "wifi_ssid"
    );


    Serial.print(
        "wifi_password: "
    );

    Serial.println(
        exists(
            "wifi_password"
        )
            ? "<stored>"
            : "<not set>"
    );


    Serial.print(
        "server_config: "
    );

    Serial.println(
        exists(
            "server_config"
        )
            ? "<stored>"
            : "<not set>"
    );


    Serial.print(
        "temperature_config: "
    );

    Serial.println(
        exists(
            "temperature_config"
        )
            ? "<stored>"
            : "<not set>"
    );


    Serial.println(
        "=================================="
    );

    Serial.println();
}