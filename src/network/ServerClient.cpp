#include "network/ServerClient.h"


ServerClient::ServerClient(
    DeviceIdentity& deviceIdentity
)
    : _deviceIdentity(deviceIdentity)
{
}


void ServerClient::begin(
    const GatewayEndpoint& endpoint
)
{
    if (!endpoint.isValid()) {
        Serial.println(
            "ServerClient: no valid server configuration."
        );

        return;
    }


    if (_configured) {
        disconnect();
        delete _webSocketClient;
        _webSocketClient = nullptr;
    }

    _endpoint = endpoint;

    _configured = true;

    _connected = false;

    _registered = false;

    _failedConnectionCycles = 0;

    _reconnectIntervalMs = INITIAL_RECONNECT_INTERVAL_MS;


    Serial.print(
        "ServerClient: configured for ws://"
    );

    Serial.print(
        _endpoint.address
    );

    Serial.print(
        ":"
    );

    Serial.print(
        _endpoint.port
    );

    Serial.println(
        _endpoint.path
    );


    connect();
}


void ServerClient::update()
{
    if (!_configured) {
        return;
    }


    if (!_connected) {
        const unsigned long now =
            millis();


        if (
            now - _lastConnectionAttemptMs >=
            _reconnectIntervalMs
        ) {
            connect();
        }


        return;
    }


    if (
        _webSocketClient == nullptr ||
        !_webSocketClient->connected()
    ) {
        if (_failedConnectionCycles < 255) ++_failedConnectionCycles;
        disconnect();

        return;
    }


    handleIncomingMessages();


    if (
        _connected &&
        !_registered &&
        millis() - _registrationSentMs >=
            REGISTRATION_TIMEOUT_MS
    ) {
        Serial.println(
            "ServerClient: registration ACK timed out."
        );

        if (_failedConnectionCycles < 255) ++_failedConnectionCycles;
        disconnect();
    }
}


bool ServerClient::isConnected() const
{
    return _connected;
}


bool ServerClient::isRegistered() const
{
    return _registered;
}


void ServerClient::onNetworkDisconnected()
{
    _reconnectIntervalMs =
        INITIAL_RECONNECT_INTERVAL_MS;

    if (
        !_connected &&
        !_registered
    ) {
        return;
    }

    Serial.println(
        "ServerClient: WiFi connection lost."
    );

    disconnect();
}

void ServerClient::stop()
{
    disconnect();
    _configured = false;
    _failedConnectionCycles = 0;
}

void ServerClient::requestReconnect()
{
    if (!_configured) return;
    disconnect();
    _lastConnectionAttemptMs = millis() - _reconnectIntervalMs;
}

uint8_t ServerClient::failedConnectionCycles() const
{
    return _failedConnectionCycles;
}


void ServerClient::connect()
{
    if (!_configured) {
        return;
    }


    _lastConnectionAttemptMs =
        millis();


    if (_webSocketClient == nullptr) {
        _webSocketClient =
            new WebSocketClient(
                _wifiClient,
                _endpoint.address.c_str(),
                _endpoint.port
            );


        if (_webSocketClient == nullptr) {
            Serial.println(
                "ServerClient: failed to allocate WebSocket client."
            );

            _connected = false;
            _registered = false;

            return;
        }
    }
    else {
        _webSocketClient->stop();
    }


    _wifiClient.stop();


    Serial.print(
        "ServerClient: connecting to ws://"
    );

    Serial.print(
        _endpoint.address
    );

    Serial.print(
        ":"
    );

    Serial.print(
        _endpoint.port
    );

    Serial.println(
        _endpoint.path
    );


    const int result =
        _webSocketClient->begin(
            _endpoint.path.c_str()
        );


    if (result != 0) {
        Serial.print(
            "ServerClient: WebSocket connection failed, result="
        );

        Serial.println(
            result
        );


        _webSocketClient->stop();


        _connected = false;

        _registered = false;

        if (_failedConnectionCycles < 255) ++_failedConnectionCycles;

        _reconnectIntervalMs *= 2;

        if (
            _reconnectIntervalMs >
            MAX_RECONNECT_INTERVAL_MS
        ) {
            _reconnectIntervalMs =
                MAX_RECONNECT_INTERVAL_MS;
        }

        Serial.print(
            "ServerClient: next reconnect in ms="
        );
        Serial.println(_reconnectIntervalMs);

        return;
    }


    _connected = true;

    _registered = false;

    _reconnectIntervalMs =
        INITIAL_RECONNECT_INTERVAL_MS;


    Serial.println(
        "ServerClient: connected."
    );


    sendRegistration();
}


void ServerClient::disconnect()
{
    if (_connected) {
        Serial.println(
            "ServerClient: disconnected."
        );
    }


    _connected = false;

    _registered = false;


    if (_webSocketClient != nullptr) {
        _webSocketClient->stop();
    }


    _wifiClient.stop();
}


void ServerClient::sendRegistration()
{
    if (
        !_connected ||
        _webSocketClient == nullptr
    ) {
        return;
    }

    String message;

    message.reserve(180);

    message += "{\"type\":\"REGISTER_SENSOR\",";
    message += "\"deviceId\":\"";
    message += _deviceIdentity.getDeviceId();
    message += "\",";
    message += "\"deviceName\":\"";
    message += _deviceIdentity.getDeviceName();
    message += "\"}";

    if (
        sendTextMessage(
            message,
            "registration"
        )
    ) {
        _registrationSentMs =
            millis();

        Serial.print(
            "ServerClient: registration sent: "
        );

        Serial.println(
            message
        );
    }
    else if (_failedConnectionCycles < 255) {
        ++_failedConnectionCycles;
    }
}


bool ServerClient::sendTextMessage(
    const String& message,
    const char* description
)
{
    if (
        !_connected ||
        _webSocketClient == nullptr
    ) {
        return false;
    }

    if (
        message.length() >
            MAX_WEBSOCKET_MESSAGE_SIZE
    ) {
        Serial.print("ServerClient: ");
        Serial.print(description);
        Serial.println(" message exceeds WebSocket TX buffer.");

        disconnect();
        return false;
    }

    const int beginResult =
        _webSocketClient->beginMessage(
            TYPE_TEXT
        );

    if (beginResult != 0) {
        Serial.print("ServerClient: failed to begin ");
        Serial.print(description);
        Serial.print(" message, result=");
        Serial.println(beginResult);

        disconnect();
        return false;
    }

    const size_t bytesWritten =
        _webSocketClient->write(
            reinterpret_cast<const uint8_t*>(
                message.c_str()
            ),
            message.length()
        );

    if (bytesWritten != message.length()) {
        Serial.print("ServerClient: incomplete ");
        Serial.print(description);
        Serial.print(" message write, bytes=");
        Serial.print(bytesWritten);
        Serial.print(" expected=");
        Serial.println(message.length());

        disconnect();
        return false;
    }

    const int endResult =
        _webSocketClient->endMessage();

    if (endResult != 0) {
        Serial.print("ServerClient: failed to end ");
        Serial.print(description);
        Serial.print(" message, result=");
        Serial.println(endResult);

        disconnect();
        return false;
    }

    return true;
}


void ServerClient::handleIncomingMessages()
{
    if (
        !_connected ||
        _webSocketClient == nullptr
    ) {
        return;
    }


    const int messageSize =
        _webSocketClient->parseMessage();


    if (messageSize <= 0) {
        return;
    }


    if (
        static_cast<size_t>(messageSize) >
            MAX_WEBSOCKET_MESSAGE_SIZE
    ) {
        Serial.print(
            "ServerClient: incoming message too large, bytes="
        );
        Serial.println(messageSize);

        disconnect();
        return;
    }


    const int messageType =
        _webSocketClient->messageType();


    if (messageType != TYPE_TEXT) {
        return;
    }


    String message;

    message.reserve(
        messageSize
    );


    while (
        _webSocketClient->available()
    ) {
        const int value =
            _webSocketClient->read();


        if (value < 0) {
            break;
        }


        message +=
            static_cast<char>(
                value
            );
    }


    Serial.print(
        "ServerClient: received: "
    );

    Serial.println(
        message
    );


    handleMessage(
        message
    );
}


void ServerClient::handleMessage(
    const String& message
)
{
    uint32_t acknowledgementSequence = 0;

    if (parseMeasurementAcknowledgement(
        message,
        acknowledgementSequence
    )) {
        _measurementAcknowledgementSequence = acknowledgementSequence;
        _hasMeasurementAcknowledgement = true;
        return;
    }

    String type;

    if (!parseMessageType(message, type)) {
        Serial.println(
            "ServerClient: invalid JSON message."
        );

        return;
    }

    if (type == "REGISTER_SENSOR_ACK") {
        if (!_registered) {
            _registered = true;

            _failedConnectionCycles = 0;


            Serial.println(
                "ServerClient: sensor registered."
            );
        }


        return;
    }


    Serial.print(
        "ServerClient: unknown message type: "
    );
    Serial.println(type);
}

bool ServerClient::parseMeasurementAcknowledgement(
    const String& message,
    uint32_t& sequence
) const
{
    size_t position = 0;
    const auto skipWhitespace = [&message, &position]() {
        while (position < message.length() &&
               (message[position] == ' ' || message[position] == '\t' ||
                message[position] == '\r' || message[position] == '\n')) ++position;
    };
    const auto consume = [&message, &position](const char* expected) {
        size_t offset = 0;
        while (expected[offset] != '\0') {
            if (position >= message.length() ||
                message[position++] != expected[offset++]) return false;
        }
        return true;
    };

    skipWhitespace();
    if (!consume("{")) return false;
    skipWhitespace();
    if (!consume("\"type\"")) return false;
    skipWhitespace();
    if (!consume(":")) return false;
    skipWhitespace();
    if (!consume("\"MEASUREMENT_ACK\"")) return false;
    skipWhitespace();
    if (!consume(",")) return false;
    skipWhitespace();
    if (!consume("\"sequence\"")) return false;
    skipWhitespace();
    if (!consume(":")) return false;
    skipWhitespace();
    if (position >= message.length() ||
        message[position] < '0' || message[position] > '9') return false;

    uint32_t value = 0;
    while (position < message.length() &&
           message[position] >= '0' && message[position] <= '9') {
        const uint8_t digit = static_cast<uint8_t>(message[position++] - '0');
        if (value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    skipWhitespace();
    if (!consume("}")) return false;
    skipWhitespace();
    if (position != message.length()) return false;
    sequence = value;
    return true;
}

bool ServerClient::parseMessageType(
    const String& message,
    String& type
) const
{
    size_t position = 0;
    const auto skipWhitespace = [&message, &position]() {
        while (position < message.length() &&
               (message[position] == ' ' || message[position] == '\t' ||
                message[position] == '\r' || message[position] == '\n')) ++position;
    };
    const auto readString = [&message, &position](String& value) {
        if (position >= message.length() || message[position++] != '"') return false;
        value = "";
        while (position < message.length()) {
            const char character = message[position++];
            if (character == '"') return true;
            if (character == '\\' || static_cast<uint8_t>(character) < 0x20) return false;
            value += character;
        }
        return false;
    };

    skipWhitespace();
    if (position >= message.length() || message[position++] != '{') return false;
    skipWhitespace();
    String key;
    if (!readString(key) || key != "type") return false;
    skipWhitespace();
    if (position >= message.length() || message[position++] != ':') return false;
    skipWhitespace();
    if (!readString(type)) return false;
    skipWhitespace();

    // The discriminator is valid both for single-field registration ACKs and
    // messages carrying further fields such as a measurement sequence.
    if (position < message.length() && message[position] == ',') return true;
    if (position >= message.length() || message[position++] != '}') return false;
    skipWhitespace();
    return position == message.length();
}

bool ServerClient::sendMeasurement(
    const OutboxEntry& measurement,
    uint32_t nowMs
)
{
    if (!_connected || !_registered || _webSocketClient == nullptr) {
        return false;
    }

    String message;
    message.reserve(220);
    if (measurement.type == MeasurementType::TEMPERATURE) {
        message += "{\"type\":\"TEMPERATURE_MEASUREMENT\",\"deviceId\":\"";
        message += _deviceIdentity.getDeviceId();
        message += "\",\"sequence\":";
        message += String(measurement.sequence);
        message += ",\"beerTemperature\":";
        message += String(measurement.payload.temperature.beerTemperature, 1);
        message += ",\"ambientTemperature\":";
        message += String(measurement.payload.temperature.ambientTemperature, 1);
        if (measurement.payload.temperature.pressureAvailable) {
            message += ",\"pressurePa\":";
            message += String(measurement.payload.temperature.pressurePa, 2);
        }
        message += ",\"measurementAgeSeconds\":";
        message += String(measurement.ageSeconds(nowMs));
    } else {
        message += "{\"type\":\"BUBBLE_ACTIVITY\",\"deviceId\":\"";
        message += _deviceIdentity.getDeviceId();
        message += "\",\"sequence\":";
        message += String(measurement.sequence);
        message += ",\"bubbleCount\":";
        message += String(measurement.payload.bubbleActivity.bubbleCount);
        message += ",\"windowSeconds\":";
        message += String(measurement.payload.bubbleActivity.windowSeconds);
        message += ",\"averagePressureDeltaPa\":";
        message += String(
            measurement.payload.bubbleActivity.averagePressureDeltaPa,
            2
        );
        message += ",\"windowEndAgeSeconds\":";
        message += String(measurement.ageSeconds(nowMs));
    }
    message += "}";
    return sendTextMessage(message, "measurement");
}

bool ServerClient::takeMeasurementAcknowledgement(uint32_t& sequence)
{
    if (!_hasMeasurementAcknowledgement) return false;
    sequence = _measurementAcknowledgementSequence;
    _hasMeasurementAcknowledgement = false;
    return true;
}
