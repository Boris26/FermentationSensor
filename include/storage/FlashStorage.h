#pragma once

#include <Arduino.h>

namespace mbed
{
class KVStore;
}

class FlashStorage
{
public:
    bool begin();

    bool setString(
        const char* key,
        const String& value
    );

    bool getString(
        const char* key,
        String& value
    );

    bool setBytes(
        const char* key,
        const void* data,
        size_t size
    );

    bool getBytes(
        const char* key,
        void* data,
        size_t size
    );

    bool remove(
        const char* key
    );

    bool exists(
        const char* key
    );

    bool clearAll();

    // Destructive erase intended only for an explicit factory-reset flow.
    // The legacy Mbed /kv/ store is cleared first so stale configuration
    // cannot be imported again after the application store is erased.
    bool factoryReset();

    // Low-level operation used by StorageMaintenance after its complete
    // in-RAM backup has been validated. It resets only the dedicated
    // application store; the boot-time migration marker is recreated later.
    bool resetForMaintenance();

    bool clearWifi();

    void debugPrintAll();

private:
    bool migrateLegacyStore();
    bool migrateLegacyKey(
        const char* key,
        bool& foundLegacyData
    );
    bool hasValidLayoutMarker() const;
    bool writeLayoutMarker();
    bool legacyStoreHasApplicationData() const;
    void cleanupLegacyStore();

    void printWriteError(
        const char* key,
        int result,
        size_t size
    ) const;
    void debugPrintEntries();

    mbed::KVStore* _store = nullptr;
    bool _ready = false;
};
