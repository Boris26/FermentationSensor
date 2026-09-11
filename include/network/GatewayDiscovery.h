#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>

#include "network/GatewayEndpoint.h"

class GatewayDiscovery
{
public:
    void start();
    void update();
    void stop();
    bool isRunning() const { return _running; }
    bool takeResult(GatewayEndpoint& endpoint);

private:
    bool sendQuery(const String& name, uint16_t type);
    void parsePacket(const uint8_t* data, size_t length);
    bool readName(const uint8_t* data, size_t length, size_t& offset, String& name) const;
    void resetRecords();
    void tryComplete();

    WiFiUDP _udp;
    bool _running = false;
    bool _hasResult = false;
    unsigned long _startedMs = 0;
    unsigned long _lastQueryMs = 0;
    String _instance;
    String _host;
    String _path;
    IPAddress _address;
    uint16_t _port = 0;
    uint16_t _version = 0;
    GatewayEndpoint _result;

    static constexpr unsigned long DISCOVERY_TIMEOUT_MS = 5000;
    static constexpr unsigned long QUERY_INTERVAL_MS = 1000;
};
