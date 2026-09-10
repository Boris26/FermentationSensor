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
        resetClient();
        _started = false;
        return;
    }

    if (!_clientActive) {
        _client = _server.available();

        if (!_client) {
            return;
        }

        _clientActive = true;
        _clientStartedMs = millis();
    }

    if (!_client.connected()) {
        resetClient();
        return;
    }

    if (
        millis() - _clientStartedMs >=
        CLIENT_TIMEOUT_MS
    ) {
        Serial.println(
            "BootstrapServer: request timed out."
        );

        resetClient();
        return;
    }

    handleClient(_client);
}


void BootstrapServer::handleClient(
    WiFiClient& client
)
{
    size_t bytesRead = 0;

    while (
        client.available() &&
        bytesRead < READ_BUDGET_BYTES
    ) {
        const char character =
            static_cast<char>(client.read());

        ++bytesRead;

        if (!_readingBody) {
            if (character == '\r') {
                continue;
            }

            if (character != '\n') {
                _headerLine += character;

                if (_headerLine.length() > 512) {
                    sendResponse(
                        client,
                        400,
                        "Bad Request",
                        "{\"error\":\"request header too long\"}"
                    );

                    resetClient();
                    return;
                }

                continue;
            }

            if (_requestLine.isEmpty()) {
                _requestLine = _headerLine;
                _headerLine = "";
                continue;
            }

            if (_headerLine.isEmpty()) {
                if (
                    _requestLine !=
                    "POST /connect HTTP/1.1"
                ) {
                    sendResponse(
                        client,
                        404,
                        "Not Found",
                        "{\"error\":\"endpoint not found\"}"
                    );

                    resetClient();
                    return;
                }

                if (
                    _contentLength <= 0 ||
                    _contentLength > 512
                ) {
                    sendResponse(
                        client,
                        400,
                        "Bad Request",
                        "{\"error\":\"invalid content length\"}"
                    );

                    resetClient();
                    return;
                }

                _body.reserve(_contentLength);
                _readingBody = true;
                continue;
            }

            if (
                _headerLine.startsWith(
                    "Content-Length:"
                )
            ) {
                String value =
                    _headerLine.substring(
                        strlen("Content-Length:")
                    );

                value.trim();
                _contentLength = value.toInt();
            }

            _headerLine = "";
            continue;
        }

        _body += character;

        if (
            _body.length() ==
            static_cast<unsigned int>(_contentLength)
        ) {
            break;
        }
    }

    if (
        !_readingBody ||
        _body.length() !=
            static_cast<unsigned int>(_contentLength)
    ) {
        return;
    }


    ServerConfiguration configuration;


    if (
        !parseConfiguration(
            _body,
            configuration
        )
    ) {
        sendResponse(
            client,
            400,
            "Bad Request",
            "{\"error\":\"invalid configuration\"}"
        );

        resetClient();
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

        resetClient();
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

    resetClient();
}


void BootstrapServer::resetClient()
{
    _client.stop();
    _client = WiFiClient();
    _requestLine = "";
    _headerLine = "";
    _body = "";
    _contentLength = 0;
    _clientStartedMs = 0;
    _clientActive = false;
    _readingBody = false;
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
