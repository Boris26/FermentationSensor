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


bool BootstrapServer::consumeConfigurationChanged()
{
    const bool changed = _configurationChanged;
    _configurationChanged = false;
    return changed;
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


    _configurationChanged = true;


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
    size_t position = 0;
    bool hasHost = false;
    bool hasPort = false;
    bool hasPath = false;

    const auto skipWhitespace = [&]() {
        while (
            position < body.length() &&
            isspace(static_cast<unsigned char>(body[position]))
        ) {
            ++position;
        }
    };

    const auto readString = [&](String& value) {
        if (
            position >= body.length() ||
            body[position++] != '"'
        ) {
            return false;
        }

        value = "";

        while (position < body.length()) {
            const char character = body[position++];

            if (character == '"') {
                return true;
            }

            if (
                character == '\\' ||
                static_cast<uint8_t>(character) < 0x20
            ) {
                return false;
            }

            value += character;
        }

        return false;
    };

    skipWhitespace();

    if (
        position >= body.length() ||
        body[position++] != '{'
    ) {
        return false;
    }

    while (true) {
        skipWhitespace();

        if (
            position < body.length() &&
            body[position] == '}'
        ) {
            ++position;
            break;
        }

        String key;

        if (!readString(key)) {
            return false;
        }

        skipWhitespace();

        if (
            position >= body.length() ||
            body[position++] != ':'
        ) {
            return false;
        }

        skipWhitespace();

        if (key == "host" || key == "path") {
            String value;

            if (!readString(value)) {
                return false;
            }

            if (key == "host") {
                if (hasHost) {
                    return false;
                }

                configuration.host = value;
                hasHost = true;
            }
            else {
                if (hasPath) {
                    return false;
                }

                configuration.path = value;
                hasPath = true;
            }
        }
        else if (key == "port") {
            if (
                hasPort ||
                position >= body.length() ||
                !isDigit(body[position])
            ) {
                return false;
            }

            uint32_t port = 0;

            while (
                position < body.length() &&
                isDigit(body[position])
            ) {
                port =
                    port * 10 +
                    (body[position++] - '0');

                if (port > 65535) {
                    return false;
                }
            }

            if (port == 0) {
                return false;
            }

            configuration.port =
                static_cast<uint16_t>(port);
            hasPort = true;
        }
        else {
            return false;
        }

        skipWhitespace();

        if (
            position < body.length() &&
            body[position] == ','
        ) {
            ++position;
            skipWhitespace();

            if (
                position >= body.length() ||
                body[position] == '}'
            ) {
                return false;
            }

            continue;
        }

        if (
            position < body.length() &&
            body[position] == '}'
        ) {
            ++position;
            break;
        }

        return false;
    }

    skipWhitespace();
    configuration.host.trim();
    configuration.path.trim();

    return
        position == body.length() &&
        hasHost &&
        hasPort &&
        hasPath &&
        configuration.isValid() &&
        configuration.path.startsWith("/");
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
