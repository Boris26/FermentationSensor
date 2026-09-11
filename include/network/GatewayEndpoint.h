#pragma once

#include <Arduino.h>

struct GatewayEndpoint
{
    String address;
    uint16_t port = 0;
    String path;
    uint16_t protocolVersion = 0;

    bool isValid() const
    {
        if (address.isEmpty() || port == 0 || !path.startsWith("/") ||
            protocolVersion != 1) {
            return false;
        }

        IPAddress parsed;
        return parsed.fromString(address.c_str());
    }
};
