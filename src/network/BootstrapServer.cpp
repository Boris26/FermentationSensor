#include "network/BootstrapServer.h"

#include <Arduino.h>
#include <WiFiNINA.h>


BootstrapServer::BootstrapServer(
    ServerConfigurationStore& configurationStore
)
    : _configurationStore(configurationStore)
{
}


void BootstrapServer::begin()
{
    if (_started) {
        return;
    }


    if (WiFi.status() != WL_CONNECTED) {
        return;
    }


    _server.begin();

    _started = true;


    Serial.println(
        "BootstrapServer: listening on port 80."
    );
}


void BootstrapServer::update()
{
    if (!_started) {
        begin();
        return;
    }


    if (WiFi.status() != WL_CONNECTED) {
        _started = false;
        return;
    }


    WiFiClient client =
        _server.available();


    if (!client) {
        return;
    }


    handleClient(
        client
    );


    client.stop();
}


void BootstrapServer::handleClient(
    WiFiClient& client
)
{
    String requestLine =
        client.readStringUntil('\n');

    requestLine.trim();


    if (
        requestLine !=
        "POST /connect HTTP/1.1"
    ) {
        sendResponse(
            client,
            404,
            "Not Found",
            "{\"error\":\"endpoint not found\"}"
        );

        return;
    }


    int contentLength = 0;


    while (client.connected()) {
        String line =
            client.readStringUntil('\n');

        line.trim();


        if (line.isEmpty()) {
            break;
        }


        if (
            line.startsWith(
                "Content-Length:"
            )
        ) {
            String value =
                line.substring(
                    strlen("Content-Length:")
                );

            value.trim();

            contentLength =
                value.toInt();
        }
    }


    if (
        contentLength <= 0 ||
        contentLength > 512
    ) {
        sendResponse(
            client,
            400,
            "Bad Request",
            "{\"error\":\"invalid content length\"}"
        );

        return;
    }


    String body;

    body.reserve(
        contentLength
    );


    const unsigned long start =
        millis();


    while (
        body.length() <
            static_cast<unsigned int>(
                contentLength
            ) &&
        millis() - start < 2000
    ) {
        while (
            client.available() &&
            body.length() <
                static_cast<unsigned int>(
                    contentLength
                )
        ) {
            body +=
                static_cast<char>(
                    client.read()
                );
        }
    }


    if (
        body.length() !=
        static_cast<unsigned int>(
            contentLength
        )
    ) {
        sendResponse(
            client,
            400,
            "Bad Request",
            "{\"error\":\"incomplete request body\"}"
        );

        return;
    }


    ServerConfiguration configuration;


    if (
        !parseConfiguration(
            body,
            configuration
        )
    ) {
        sendResponse(
            client,
            400,
            "Bad Request",
            "{\"error\":\"invalid configuration\"}"
        );

        return;
    }


    if (
        !_configurationStore.save(
            configuration
        )
    ) {
        sendResponse(
            client,
            500,
            "Internal Server Error",
            "{\"error\":\"configuration could not be saved\"}"
        );

        return;
    }


    Serial.print(
        "BootstrapServer: backend configured: "
    );

    Serial.print(
        configuration.host
    );

    Serial.print(
        ":"
    );

    Serial.print(
        configuration.port
    );

    Serial.println(
        configuration.path
    );


    sendResponse(
        client,
        200,
        "OK",
        "{\"status\":\"configured\"}"
    );
}


bool BootstrapServer::parseConfiguration(
    const String& body,
    ServerConfiguration& configuration
)
{
    if (
        !readJsonString(
            body,
            "host",
            configuration.host
        )
    ) {
        return false;
    }


    if (
        !readJsonNumber(
            body,
            "port",
            configuration.port
        )
    ) {
        return false;
    }


    if (
        !readJsonString(
            body,
            "path",
            configuration.path
        )
    ) {
        return false;
    }


    configuration.host.trim();
    configuration.path.trim();


    if (
        !configuration.path.startsWith("/")
    ) {
        return false;
    }


    return configuration.isValid();
}


bool BootstrapServer::readJsonString(
    const String& json,
    const char* key,
    String& value
)
{
    const String search =
        String("\"") +
        key +
        "\"";


    int position =
        json.indexOf(search);


    if (position < 0) {
        return false;
    }


    position =
        json.indexOf(
            ':',
            position +
                search.length()
        );


    if (position < 0) {
        return false;
    }


    const int startQuote =
        json.indexOf(
            '"',
            position + 1
        );


    if (startQuote < 0) {
        return false;
    }


    const int endQuote =
        json.indexOf(
            '"',
            startQuote + 1
        );


    if (endQuote < 0) {
        return false;
    }


    value =
        json.substring(
            startQuote + 1,
            endQuote
        );


    return true;
}


bool BootstrapServer::readJsonNumber(
    const String& json,
    const char* key,
    uint16_t& value
)
{
    const String search =
        String("\"") +
        key +
        "\"";


    int position =
        json.indexOf(search);


    if (position < 0) {
        return false;
    }


    position =
        json.indexOf(
            ':',
            position +
                search.length()
        );


    if (position < 0) {
        return false;
    }


    int start =
        position + 1;


    while (
        start < json.length() &&
        (
            json[start] == ' ' ||
            json[start] == '\t'
        )
    ) {
        ++start;
    }


    int end = start;


    while (
        end < json.length() &&
        isDigit(json[end])
    ) {
        ++end;
    }


    if (end == start) {
        return false;
    }


    const long parsedValue =
        json.substring(
            start,
            end
        ).toInt();


    if (
        parsedValue <= 0 ||
        parsedValue > 65535
    ) {
        return false;
    }


    value =
        static_cast<uint16_t>(
            parsedValue
        );


    return true;
}


void BootstrapServer::sendResponse(
    WiFiClient& client,
    int statusCode,
    const char* statusText,
    const char* body
)
{
    client.print(
        "HTTP/1.1 "
    );

    client.print(
        statusCode
    );

    client.print(
        " "
    );

    client.println(
        statusText
    );


    client.println(
        "Content-Type: application/json"
    );

    client.println(
        "Connection: close"
    );

    client.print(
        "Content-Length: "
    );

    client.println(
        strlen(body)
    );

    client.println();

    client.print(
        body
    );
}