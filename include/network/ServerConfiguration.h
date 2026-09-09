#pragma once

#include <Arduino.h>

struct ServerConfiguration
{
    String host;
    uint16_t port = 0;
    String path;

    bool isValid() const
    {
        return
            !host.isEmpty() &&
            port > 0 &&
            !path.isEmpty();
    }
};