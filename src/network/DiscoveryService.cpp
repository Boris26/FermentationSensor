#include "network/DiscoveryService.h"

#include <Arduino.h>
#include <WiFiNINA.h>

namespace
{
constexpr uint16_t DISCOVERY_PORT = 4210;

constexpr char DISCOVERY_MESSAGE[] =
    "DISCOVER_FERMENTATION_SENSORS";

constexpr size_t PACKET_BUFFER_SIZE = 128;
constexpr size_t RESPONSE_BUFFER_SIZE = 256;
}


DiscoveryService::DiscoveryService(
    DeviceIdentity& deviceIdentity
)
    : _deviceIdentity(deviceIdentity)
{
}


void DiscoveryService::begin()
{
    if (_started) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (_udp.begin(DISCOVERY_PORT) == 0) {
        Serial.println(
            "DiscoveryService: failed to start UDP listener."
        );

        return;
    }

    _started = true;

    Serial.print(
        "DiscoveryService: listening on UDP port "
    );

    Serial.println(
        DISCOVERY_PORT
    );
}


void DiscoveryService::update()
{
    if (!_started) {
        begin();

        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        _udp.stop();

        _started = false;

        Serial.println(
            "DiscoveryService: WiFi disconnected."
        );

        return;
    }


    const int packetSize =
        _udp.parsePacket();


    if (packetSize <= 0) {
        return;
    }


    char buffer[PACKET_BUFFER_SIZE] = {};


    const int bytesRead =
        _udp.read(
            buffer,
            sizeof(buffer) - 1
        );


    if (bytesRead <= 0) {
        return;
    }


    buffer[bytesRead] = '\0';


    handlePacket(
        buffer,
        _udp.remoteIP(),
        _udp.remotePort()
    );
}


void DiscoveryService::handlePacket(
    const char* message,
    IPAddress remoteIp,
    uint16_t remotePort
)
{
    if (
        strcmp(
            message,
            DISCOVERY_MESSAGE
        ) != 0
    ) {
        return;
    }


    Serial.print(
        "DiscoveryService: discovery request from "
    );

    Serial.print(
        remoteIp
    );

    Serial.print(
        ":"
    );

    Serial.println(
        remotePort
    );


    sendDiscoveryResponse(
        remoteIp,
        remotePort
    );
}


void DiscoveryService::sendDiscoveryResponse(
    IPAddress remoteIp,
    uint16_t remotePort
)
{
    char response[
        RESPONSE_BUFFER_SIZE
    ] = {};


    snprintf(
        response,
        sizeof(response),
        "{"
        "\"type\":\"fermentation-sensor\","
        "\"deviceId\":\"%s\","
        "\"deviceName\":\"%s\""
        "}",
        _deviceIdentity
            .getDeviceId()
            .c_str(),
        _deviceIdentity
            .getDeviceName()
            .c_str()
    );


    if (
        _udp.beginPacket(
            remoteIp,
            remotePort
        ) == 0
    ) {
        Serial.println(
            "DiscoveryService: failed to start response."
        );

        return;
    }


    _udp.write(
        reinterpret_cast<const uint8_t*>(
            response
        ),
        strlen(response)
    );


    if (_udp.endPacket() == 0) {
        Serial.println(
            "DiscoveryService: failed to send response."
        );

        return;
    }


    Serial.print(
        "DiscoveryService: response sent to "
    );

    Serial.print(
        remoteIp
    );

    Serial.print(
        ":"
    );

    Serial.println(
        remotePort
    );
}