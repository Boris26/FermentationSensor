#pragma once

#include <Arduino.h>

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
    bool factoryReset();

    // Low-level operation used by StorageMaintenance after its complete
    // in-RAM backup has been validated. It is deliberately not called by
    // begin() or any normal boot path.
    bool resetForMaintenance();

    bool clearWifi();

    void debugPrintAll();

private:
    void printWriteError(const String& key, int result, size_t size) const;
    void debugPrintEntries();

    String buildKey(
        const char* key
    ) const;
};
