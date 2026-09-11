#include "storage/FlashStorage.h"

#include <Arduino.h>
#include <kvstore_global_api.h>

namespace
{
constexpr int KV_SUCCESS = 0;

constexpr size_t STRING_BUFFER_SIZE = 128;

constexpr char KV_PREFIX[] = "/kv/";
constexpr int KV_ERROR_MEDIA_FULL = -2130771701;
constexpr size_t KV_KEY_BUFFER_SIZE = 128;
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
        printWriteError(fullKey, result, value.length() + 1);

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
        printWriteError(fullKey, result, size);

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
    Serial.println("========== FlashStorage ==========");
    debugPrintEntries();
    Serial.println("==================================");
    Serial.println();
}


void FlashStorage::printWriteError(
    const String& key,
    int result,
    size_t size
) const
{
    Serial.print("FlashStorage: failed to write key: ");
    Serial.print(key);
    Serial.print(" result=");
    Serial.println(result);
    Serial.print("FlashStorage: data size=");
    Serial.println(size);

    if (result == KV_ERROR_MEDIA_FULL) {
        Serial.println(
            "FlashStorage: MBED_ERROR_MEDIA_FULL; KVStore has no space for this write."
        );
    }
}


void FlashStorage::debugPrintEntries()
{
    kv_iterator_t iterator;
    const int openResult = kv_iterator_open(&iterator, KV_PREFIX);
    size_t count = 0;
    size_t liveDataBytes = 0;

    if (openResult != KV_SUCCESS) {
        Serial.print("KV_ITERATOR_OPEN_FAILED,");
        Serial.println(openResult);
        Serial.println("KV_ENTRY_COUNT,0");
        Serial.println("KV_LIVE_DATA_BYTES,0");
        return;
    }

    char key[KV_KEY_BUFFER_SIZE] = {};
    while (kv_iterator_next(iterator, key, sizeof(key)) == KV_SUCCESS) {
        kv_info_t info = {};
        const int infoResult = kv_get_info(key, &info);
        if (infoResult != KV_SUCCESS) {
            Serial.print("KV_ENTRY_INFO_FAILED,");
            Serial.print(key);
            Serial.print(',');
            Serial.println(infoResult);
            continue;
        }

        Serial.print("KV_ENTRY,");
        Serial.print(key);
        Serial.print(',');
        Serial.print(static_cast<unsigned long>(info.size));
        Serial.print(',');
        Serial.println(static_cast<unsigned long>(info.flags));
        ++count;
        liveDataBytes += info.size;
    }

    const int closeResult = kv_iterator_close(iterator);
    if (closeResult != KV_SUCCESS) {
        Serial.print("KV_ITERATOR_CLOSE_FAILED,");
        Serial.println(closeResult);
    }

    Serial.print("KV_ENTRY_COUNT,");
    Serial.println(static_cast<unsigned long>(count));
    Serial.print("KV_LIVE_DATA_BYTES,");
    Serial.println(static_cast<unsigned long>(liveDataBytes));
}
