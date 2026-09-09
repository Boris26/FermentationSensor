#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>

#include "device/DeviceIdentity.h"

class DiscoveryService
{
public:
    explicit DiscoveryService(
        DeviceIdentity& deviceIdentity
    );

    void begin();
    void update();

private:
    void handlePacket(
        const char* message,
        IPAddress remoteIp,
        uint16_t remotePort
    );

    void sendDiscoveryResponse(
        IPAddress remoteIp,
        uint16_t remotePort
    );

    DeviceIdentity& _deviceIdentity;

    WiFiUDP _udp;

    bool _started = false;
};