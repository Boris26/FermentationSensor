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
};