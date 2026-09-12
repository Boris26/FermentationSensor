#include "storage/FlashStorage.h"

#include <Arduino.h>
#include <cstring>
#include <FlashIAP/FlashIAPBlockDevice.h>
#include <kv_config.h>
#include <kvstore/KVStore.h>
#include <kvstore_global_api.h>
#include <tdbstore/TDBStore.h>

extern "C" uint8_t __flash_binary_end;

namespace
{
constexpr int KV_SUCCESS = 0;
constexpr int KV_ERROR_MEDIA_FULL = -2130771701;

constexpr size_t STRING_BUFFER_SIZE = 128;
constexpr size_t KV_KEY_BUFFER_SIZE = 128;
constexpr size_t LEGACY_MIGRATION_BUFFER_SIZE = 256;

constexpr mbed::bd_size_t APPLICATION_STORAGE_SIZE_BYTES =
    64U * 1024U;

constexpr char LEGACY_KV_PREFIX[] = "/kv/";
constexpr char STORAGE_LAYOUT_KEY[] = "storage_layout_v1";
constexpr uint32_t STORAGE_LAYOUT_MAGIC = 0x46534B31UL; // FSK1
constexpr uint16_t STORAGE_LAYOUT_VERSION = 1;

struct StorageLayoutMarker
{
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
};

static_assert(
    sizeof(StorageLayoutMarker) == 8,
    "storage layout marker changed"
);

constexpr const char* LEGACY_APPLICATION_KEYS[] = {
    "device_config",
    "wifi_config",
    "temperature_config",
    "gateway_cache",
    "measurement_sequence_v1",
    "sensor_config_v1",
    // Older firmware keys are copied too so their existing store-specific
    // migration logic can still run after moving to the dedicated store.
    "wifi_ssid",
    "wifi_password",
    "device_uuid",
    "device_name",
    "server_config"
};

String legacyKey(const char* key)
{
    return String(LEGACY_KV_PREFIX) + key;
}
}


bool FlashStorage::begin()
{
    if (_ready && _store != nullptr) return true;

    mbed::bd_addr_t legacyStart = 0;
    mbed::bd_size_t legacySize = 0;

    const int boundsResult =
        kv_get_default_flash_addresses(
            &legacyStart,
            &legacySize
        );

    if (boundsResult != KV_SUCCESS) {
        Serial.print("FlashStorage: legacy KV bounds unavailable result=");
        Serial.println(boundsResult);
        return false;
    }

    if (legacyStart < APPLICATION_STORAGE_SIZE_BYTES) {
        Serial.println("FlashStorage: application storage address underflow.");
        return false;
    }

    const mbed::bd_addr_t applicationStart =
        legacyStart - APPLICATION_STORAGE_SIZE_BYTES;

    const uintptr_t firmwareEnd =
        reinterpret_cast<uintptr_t>(&__flash_binary_end);

    if (firmwareEnd > static_cast<uintptr_t>(applicationStart)) {
        Serial.print("FlashStorage: application store overlaps firmware; firmwareEnd=0x");
        Serial.print(static_cast<unsigned long>(firmwareEnd), HEX);
        Serial.print(" storageStart=0x");
        Serial.println(static_cast<unsigned long>(applicationStart), HEX);
        return false;
    }

    // The dedicated application store lives immediately below Mbed's legacy
    // /kv/ area. Keeping the regions separate allows a copy-and-verify
    // migration without erasing the only copy of device identity, WiFi,
    // temperature assignments, gateway cache, or sequence state first.
    static FlashIAPBlockDevice applicationBlockDevice(
        static_cast<uint32_t>(applicationStart),
        static_cast<uint32_t>(APPLICATION_STORAGE_SIZE_BYTES)
    );

    static mbed::TDBStore applicationStore(
        &applicationBlockDevice
    );

    const int initResult =
        applicationStore.init();

    if (initResult != KV_SUCCESS) {
        Serial.print("FlashStorage: application TDBStore init failed result=");
        Serial.println(initResult);
        return false;
    }

    _store = &applicationStore;
    _ready = true;

    Serial.print("APP_KV_START,0x");
    Serial.println(static_cast<unsigned long>(applicationStart), HEX);
    Serial.print("APP_KV_SIZE,");
    Serial.println(static_cast<unsigned long>(APPLICATION_STORAGE_SIZE_BYTES));
    Serial.print("LEGACY_KV_START,0x");
    Serial.println(static_cast<unsigned long>(legacyStart), HEX);
    Serial.print("LEGACY_KV_SIZE,");
    Serial.println(static_cast<unsigned long>(legacySize));

    if (!migrateLegacyStore()) {
        Serial.println("FlashStorage: legacy migration failed.");
        _ready = false;
        return false;
    }

    cleanupLegacyStore();

    Serial.println("FlashStorage: ready.");
    return true;
}


bool FlashStorage::setString(
    const char* key,
    const String& value
)
{
    if (!_ready || _store == nullptr || key == nullptr) return false;

    const int result =
        _store->set(
            key,
            value.c_str(),
            value.length() + 1,
            0
        );

    if (result != KV_SUCCESS) {
        printWriteError(key, result, value.length() + 1);
        return false;
    }

    return true;
}


bool FlashStorage::getString(
    const char* key,
    String& value
)
{
    if (!_ready || _store == nullptr || key == nullptr) return false;

    mbed::KVStore::info_t info = {};
    if (_store->get_info(key, &info) != KV_SUCCESS) return false;

    if (info.size == 0) {
        value = "";
        return true;
    }

    if (info.size > STRING_BUFFER_SIZE) {
        Serial.print("FlashStorage: stored string too large for key: ");
        Serial.println(key);
        return false;
    }

    char buffer[STRING_BUFFER_SIZE] = {};
    size_t actualSize = 0;

    const int result =
        _store->get(
            key,
            buffer,
            sizeof(buffer),
            &actualSize
        );

    if (result != KV_SUCCESS || actualSize != info.size) return false;

    buffer[sizeof(buffer) - 1] = '\0';
    value = String(buffer);
    return true;
}


bool FlashStorage::setBytes(
    const char* key,
    const void* data,
    size_t size
)
{
    if (
        !_ready ||
        _store == nullptr ||
        key == nullptr ||
        data == nullptr ||
        size == 0
    ) {
        return false;
    }

    const int result =
        _store->set(
            key,
            data,
            size,
            0
        );

    if (result != KV_SUCCESS) {
        printWriteError(key, result, size);
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
        !_ready ||
        _store == nullptr ||
        key == nullptr ||
        data == nullptr ||
        size == 0
    ) {
        return false;
    }

    size_t actualSize = 0;

    const int result =
        _store->get(
            key,
            data,
            size,
            &actualSize
        );

    if (result != KV_SUCCESS) return false;

    if (actualSize != size) {
        Serial.print("FlashStorage: unexpected data size for key: ");
        Serial.println(key);
        return false;
    }

    return true;
}


bool FlashStorage::remove(
    const char* key
)
{
    if (!_ready || _store == nullptr || key == nullptr) return false;

    const int result =
        _store->remove(key);

    if (result != KV_SUCCESS) {
        Serial.print("FlashStorage: failed to remove key: ");
        Serial.print(key);
        Serial.print(" result=");
        Serial.println(result);
        return false;
    }

    return true;
}


bool FlashStorage::exists(
    const char* key
)
{
    if (!_ready || _store == nullptr || key == nullptr) return false;

    mbed::KVStore::info_t info = {};
    return _store->get_info(key, &info) == KV_SUCCESS;
}


bool FlashStorage::clearAll()
{
    return factoryReset();
}


bool FlashStorage::factoryReset()
{
    if (!_ready || _store == nullptr) return false;

    Serial.println("FlashStorage: clearing persistent storage...");

    // Clear the legacy source first. If this fails, keep the dedicated store
    // untouched so a later reboot can never import stale credentials or an old
    // measurement-sequence range after a partial factory reset.
    const int legacyResult =
        kv_reset(LEGACY_KV_PREFIX);

    if (legacyResult != KV_SUCCESS) {
        Serial.print("FlashStorage: legacy clear failed result=");
        Serial.println(legacyResult);
        return false;
    }

    const int result =
        _store->reset();

    if (result != KV_SUCCESS) {
        Serial.print("FlashStorage: application store clear failed result=");
        Serial.println(result);
        return false;
    }

    Serial.println("FlashStorage: persistent storage cleared.");
    return true;
}


bool FlashStorage::resetForMaintenance()
{
    return
        _ready &&
        _store != nullptr &&
        _store->reset() == KV_SUCCESS;
}


bool FlashStorage::clearWifi()
{
    if (!_ready || _store == nullptr) return false;

    constexpr const char* WIFI_KEYS[] = {
        "wifi_config",
        "wifi_ssid",
        "wifi_password"
    };

    for (const char* key : WIFI_KEYS) {
        if (exists(key) && !remove(key)) return false;
    }

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


bool FlashStorage::migrateLegacyStore()
{
    if (!_ready || _store == nullptr) return false;

    if (hasValidLayoutMarker()) {
        return true;
    }

    Serial.println("FLASH_STORAGE_MIGRATION_START");

    bool foundLegacyData = false;
    for (const char* key : LEGACY_APPLICATION_KEYS) {
        if (!migrateLegacyKey(key, foundLegacyData)) {
            Serial.print("FLASH_STORAGE_MIGRATION_FAILED,");
            Serial.println(key);
            return false;
        }
    }

    if (!writeLayoutMarker()) {
        Serial.println("FLASH_STORAGE_MIGRATION_FAILED,LAYOUT_MARKER");
        return false;
    }

    Serial.println(
        foundLegacyData
            ? "FLASH_STORAGE_MIGRATION_SUCCESS"
            : "FLASH_STORAGE_MIGRATION_NOT_REQUIRED"
    );

    return true;
}


bool FlashStorage::migrateLegacyKey(
    const char* key,
    bool& foundLegacyData
)
{
    const String fullLegacyKey =
        legacyKey(key);

    kv_info_t legacyInfo = {};
    const int infoResult =
        kv_get_info(
            fullLegacyKey.c_str(),
            &legacyInfo
        );

    if (infoResult != KV_SUCCESS) {
        return true;
    }

    foundLegacyData = true;

    if (
        legacyInfo.size == 0 ||
        legacyInfo.size > LEGACY_MIGRATION_BUFFER_SIZE
    ) {
        Serial.print("FlashStorage: legacy record too large: ");
        Serial.println(key);
        return false;
    }

    uint8_t legacyData[LEGACY_MIGRATION_BUFFER_SIZE] = {};
    size_t legacyActualSize = 0;

    if (
        kv_get(
            fullLegacyKey.c_str(),
            legacyData,
            sizeof(legacyData),
            &legacyActualSize
        ) != KV_SUCCESS ||
        legacyActualSize != legacyInfo.size
    ) {
        return false;
    }

    mbed::KVStore::info_t applicationInfo = {};
    if (_store->get_info(key, &applicationInfo) == KV_SUCCESS) {
        // This can happen after power loss during a previous migration. Never
        // overwrite an already copied key. Verify it instead, which also keeps
        // measurement sequence state from ever moving backwards.
        if (
            applicationInfo.size != legacyActualSize ||
            applicationInfo.size > LEGACY_MIGRATION_BUFFER_SIZE
        ) {
            return false;
        }

        uint8_t applicationData[LEGACY_MIGRATION_BUFFER_SIZE] = {};
        size_t applicationActualSize = 0;
        if (
            _store->get(
                key,
                applicationData,
                sizeof(applicationData),
                &applicationActualSize
            ) != KV_SUCCESS ||
            applicationActualSize != legacyActualSize ||
            memcmp(
                applicationData,
                legacyData,
                legacyActualSize
            ) != 0
        ) {
            return false;
        }

        return true;
    }

    if (
        _store->set(
            key,
            legacyData,
            legacyActualSize,
            0
        ) != KV_SUCCESS
    ) {
        return false;
    }

    Serial.print("FLASH_STORAGE_MIGRATED,");
    Serial.print(key);
    Serial.print(',');
    Serial.println(static_cast<unsigned long>(legacyActualSize));

    return true;
}


bool FlashStorage::hasValidLayoutMarker() const
{
    if (!_ready || _store == nullptr) return false;

    StorageLayoutMarker marker = {};
    size_t actualSize = 0;

    if (
        _store->get(
            STORAGE_LAYOUT_KEY,
            &marker,
            sizeof(marker),
            &actualSize
        ) != KV_SUCCESS ||
        actualSize != sizeof(marker)
    ) {
        return false;
    }

    return
        marker.magic == STORAGE_LAYOUT_MAGIC &&
        marker.version == STORAGE_LAYOUT_VERSION;
}


bool FlashStorage::writeLayoutMarker()
{
    StorageLayoutMarker marker = {};
    marker.magic = STORAGE_LAYOUT_MAGIC;
    marker.version = STORAGE_LAYOUT_VERSION;

    return
        _store != nullptr &&
        _store->set(
            STORAGE_LAYOUT_KEY,
            &marker,
            sizeof(marker),
            0
        ) == KV_SUCCESS;
}


bool FlashStorage::legacyStoreHasApplicationData() const
{
    for (const char* key : LEGACY_APPLICATION_KEYS) {
        const String fullLegacyKey =
            legacyKey(key);

        kv_info_t info = {};
        if (
            kv_get_info(
                fullLegacyKey.c_str(),
                &info
            ) == KV_SUCCESS
        ) {
            return true;
        }
    }

    return false;
}


void FlashStorage::cleanupLegacyStore()
{
    if (!legacyStoreHasApplicationData()) return;

    // The new store has a verified layout marker before this cleanup is
    // attempted, so failure here is non-fatal and is retried on a later boot.
    const int result =
        kv_reset(LEGACY_KV_PREFIX);

    if (result == KV_SUCCESS) {
        Serial.println("FLASH_STORAGE_LEGACY_CLEANUP_OK");
        return;
    }

    Serial.print("FLASH_STORAGE_LEGACY_CLEANUP_FAILED,");
    Serial.println(result);
}


void FlashStorage::printWriteError(
    const char* key,
    int result,
    size_t size
) const
{
    Serial.print("FlashStorage: failed to write key: /app/");
    Serial.print(key);
    Serial.print(" result=");
    Serial.println(result);
    Serial.print("FlashStorage: data size=");
    Serial.println(size);

    if (result == KV_ERROR_MEDIA_FULL) {
        Serial.println(
            "FlashStorage: MBED_ERROR_MEDIA_FULL; application KVStore has no space for this write."
        );
    }
}


void FlashStorage::debugPrintEntries()
{
    if (!_ready || _store == nullptr) {
        Serial.println("KV_ITERATOR_OPEN_FAILED,STORE_NOT_READY");
        Serial.println("KV_ENTRY_COUNT,0");
        Serial.println("KV_LIVE_DATA_BYTES,0");
        return;
    }

    mbed::KVStore::iterator_t iterator;
    const int openResult =
        _store->iterator_open(&iterator);

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
    while (
        _store->iterator_next(
            iterator,
            key,
            sizeof(key)
        ) == KV_SUCCESS
    ) {
        mbed::KVStore::info_t info = {};
        const int infoResult =
            _store->get_info(
                key,
                &info
            );

        if (infoResult != KV_SUCCESS) {
            Serial.print("KV_ENTRY_INFO_FAILED,");
            Serial.print(key);
            Serial.print(',');
            Serial.println(infoResult);
            continue;
        }

        Serial.print("KV_ENTRY,/app/");
        Serial.print(key);
        Serial.print(',');
        Serial.print(static_cast<unsigned long>(info.size));
        Serial.print(',');
        Serial.println(static_cast<unsigned long>(info.flags));
        ++count;
        liveDataBytes += info.size;
    }

    const int closeResult =
        _store->iterator_close(iterator);

    if (closeResult != KV_SUCCESS) {
        Serial.print("KV_ITERATOR_CLOSE_FAILED,");
        Serial.println(closeResult);
    }

    Serial.print("KV_ENTRY_COUNT,");
    Serial.println(static_cast<unsigned long>(count));
    Serial.print("KV_LIVE_DATA_BYTES,");
    Serial.println(static_cast<unsigned long>(liveDataBytes));
}
