#include "network/GatewayDiscovery.h"

namespace {
constexpr char SERVICE_NAME[] = "_brewferment._tcp.local";
constexpr uint16_t MDNS_PORT = 5353;
constexpr uint16_t DNS_PTR = 12;
constexpr uint16_t DNS_TXT = 16;
constexpr uint16_t DNS_A = 1;
constexpr uint16_t DNS_SRV = 33;
constexpr uint16_t DNS_ANY = 255;
constexpr size_t MAX_PACKET = 768;
uint16_t read16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
uint32_t read32(const uint8_t* p) { return (uint32_t(read16(p)) << 16) | read16(p + 2); }
void write16(uint8_t* p, uint16_t value) { p[0] = value >> 8; p[1] = value & 0xff; }
}

void GatewayDiscovery::start()
{
    stop();
    if (WiFi.status() != WL_CONNECTED) return;
    resetRecords();
    if (_udp.beginMulticast(IPAddress(224, 0, 0, 251), MDNS_PORT) == 0) {
        Serial.println("GatewayDiscovery: unable to open mDNS socket.");
        return;
    }
    _running = true;
    _startedMs = millis();
    _lastQueryMs = 0;
    Serial.println("GatewayDiscovery: searching _brewferment._tcp.local.");
    sendQuery(SERVICE_NAME, DNS_PTR);
}

void GatewayDiscovery::stop()
{
    if (_running) _udp.stop();
    _running = false;
}

void GatewayDiscovery::resetRecords()
{
    _hasResult = false; _instance = ""; _host = ""; _path = "";
    _address = IPAddress(); _port = 0; _version = 0; _result = {};
}

void GatewayDiscovery::update()
{
    if (!_running) return;
    if (WiFi.status() != WL_CONNECTED || millis() - _startedMs >= DISCOVERY_TIMEOUT_MS) {
        Serial.println("GatewayDiscovery: no valid service found.");
        stop();
        return;
    }
    int packetSize = _udp.parsePacket();
    if (packetSize > 0 && packetSize <= int(MAX_PACKET)) {
        uint8_t packet[MAX_PACKET];
        int count = _udp.read(packet, packetSize);
        if (count > 0) parsePacket(packet, count);
    } else if (packetSize > int(MAX_PACKET)) {
        while (_udp.available()) _udp.read();
    }
    if (!_running) return;
    if (millis() - _lastQueryMs >= QUERY_INTERVAL_MS) {
        if (_instance.isEmpty()) sendQuery(SERVICE_NAME, DNS_PTR);
        else if (_host.isEmpty() || _path.isEmpty() || _port == 0) sendQuery(_instance, DNS_ANY);
        else if (_address == IPAddress()) sendQuery(_host, DNS_A);
    }
}

bool GatewayDiscovery::takeResult(GatewayEndpoint& endpoint)
{
    if (!_hasResult) return false;
    endpoint = _result;
    _hasResult = false;
    return true;
}

bool GatewayDiscovery::sendQuery(const String& name, uint16_t type)
{
    uint8_t packet[256] = {};
    write16(packet + 4, 1);
    size_t offset = 12, start = 0;
    while (start < name.length()) {
        int dot = name.indexOf('.', start);
        size_t end = dot < 0 ? name.length() : size_t(dot);
        size_t labelLength = end - start;
        if (labelLength == 0 || labelLength > 63 || offset + labelLength + 5 > sizeof(packet)) return false;
        packet[offset++] = labelLength;
        memcpy(packet + offset, name.c_str() + start, labelLength);
        offset += labelLength;
        start = end + 1;
    }
    packet[offset++] = 0;
    write16(packet + offset, type); offset += 2;
    write16(packet + offset, 1); offset += 2;
    _lastQueryMs = millis();
    if (!_udp.beginPacket(IPAddress(224, 0, 0, 251), MDNS_PORT)) return false;
    _udp.write(packet, offset);
    return _udp.endPacket() == 1;
}

bool GatewayDiscovery::readName(const uint8_t* data, size_t length, size_t& offset, String& name) const
{
    size_t cursor = offset, returnOffset = offset;
    bool jumped = false;
    unsigned int jumps = 0;
    name = "";
    while (cursor < length && jumps++ < 32) {
        uint8_t label = data[cursor++];
        if (label == 0) { offset = jumped ? returnOffset : cursor; return true; }
        if ((label & 0xc0) == 0xc0) {
            if (cursor >= length) return false;
            size_t pointer = ((label & 0x3f) << 8) | data[cursor++];
            if (pointer >= length) return false;
            if (!jumped) { returnOffset = cursor; jumped = true; }
            cursor = pointer;
            continue;
        }
        if ((label & 0xc0) || cursor + label > length) return false;
        if (!name.isEmpty()) name += '.';
        for (uint8_t i = 0; i < label; ++i) name += char(data[cursor++]);
    }
    return false;
}

void GatewayDiscovery::parsePacket(const uint8_t* data, size_t length)
{
    if (length < 12) return;
    uint32_t count = read16(data + 4) + read16(data + 6) + read16(data + 8) + read16(data + 10);
    size_t offset = 12;
    for (uint16_t i = 0; i < read16(data + 4); ++i) {
        String ignored;
        if (!readName(data, length, offset, ignored) || offset + 4 > length) return;
        offset += 4;
    }
    count -= read16(data + 4);
    for (uint32_t i = 0; i < count; ++i) {
        String owner;
        if (!readName(data, length, offset, owner) || offset + 10 > length) return;
        uint16_t type = read16(data + offset); offset += 2;
        offset += 2; // class
        (void) read32(data + offset); offset += 4;
        uint16_t rdLength = read16(data + offset); offset += 2;
        size_t end = offset + rdLength;
        if (end > length) return;
        if (type == DNS_PTR && owner.equalsIgnoreCase(SERVICE_NAME)) {
            size_t valueOffset = offset; String value;
            if (readName(data, length, valueOffset, value)) _instance = value;
        } else if (!_instance.isEmpty() && owner.equalsIgnoreCase(_instance) && type == DNS_SRV && rdLength >= 7) {
            _port = read16(data + offset + 4);
            size_t valueOffset = offset + 6; String value;
            if (readName(data, length, valueOffset, value)) _host = value;
        } else if (!_instance.isEmpty() && owner.equalsIgnoreCase(_instance) && type == DNS_TXT) {
            size_t cursor = offset;
            while (cursor < end) {
                uint8_t txtLength = data[cursor++];
                if (cursor + txtLength > end) break;
                String item;
                for (uint8_t n = 0; n < txtLength; ++n) item += char(data[cursor++]);
                if (item.startsWith("path=")) _path = item.substring(5);
                else if (item.startsWith("version=")) {
                    String value = item.substring(8);
                    _version = value == "1" ? 1 : 0xffff;
                }
            }
        } else if (!_host.isEmpty() && owner.equalsIgnoreCase(_host) && type == DNS_A && rdLength == 4) {
            _address = IPAddress(data[offset], data[offset + 1], data[offset + 2], data[offset + 3]);
        }
        offset = end;
    }
    tryComplete();
}

void GatewayDiscovery::tryComplete()
{
    GatewayEndpoint candidate;
    candidate.address = _address.toString();
    candidate.port = _port;
    candidate.path = _path;
    candidate.protocolVersion = _version;
    if (!candidate.isValid()) return;
    _result = candidate;
    _hasResult = true;
    Serial.println("GatewayDiscovery: valid gateway found.");
    stop();
}
