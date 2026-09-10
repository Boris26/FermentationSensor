#include "network/ServerClient.h"


ServerClient::ServerClient(
    DeviceIdentity& deviceIdentity
)
    : _deviceIdentity(deviceIdentity)
{
}


void ServerClient::begin(
    const ServerConfiguration& configuration
)
{
    if (!configuration.isValid()) {
        Serial.println(
            "ServerClient: no valid server configuration."
        );

        return;
    }


    _configuration =
        configuration;

    _configured = true;

    _connected = false;

    _registered = false;


    Serial.print(
        "ServerClient: configured for ws://"
    );

    Serial.print(
        _configuration.host
    );

    Serial.print(
        ":"
    );

    Serial.print(
        _configuration.port
    );

    Serial.println(
        _configuration.path
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
                _configuration.host.c_str(),
                _configuration.port
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
        _configuration.host
    );

    Serial.print(
        ":"
    );

    Serial.print(
        _configuration.port
    );

    Serial.println(
        _configuration.path
    );


    const int result =
        _webSocketClient->begin(
            _configuration.path.c_str()
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


bool ServerClient::parseMessageType(
    const String& message,
    String& type
) const
{
    size_t position = 0;

    const auto skipWhitespace =
        [&message, &position]()
        {
            while (
                position < message.length() &&
                (
                    message[position] == ' ' ||
                    message[position] == '\t' ||
                    message[position] == '\r' ||
                    message[position] == '\n'
                )
            ) {
                ++position;
            }
        };

    const auto readString =
        [&message, &position](String& value)
        {
            if (
                position >= message.length() ||
                message[position] != '"'
            ) {
                return false;
            }

            ++position;
            value = "";

            while (position < message.length()) {
                const char character =
                    message[position++];

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
        position >= message.length() ||
        message[position++] != '{'
    ) {
        return false;
    }

    skipWhitespace();

    String key;

    if (!readString(key)) {
        return false;
    }

    skipWhitespace();

    if (
        position >= message.length() ||
        message[position++] != ':'
    ) {
        return false;
    }

    skipWhitespace();

    if (
        key != "type" ||
        !readString(type)
    ) {
        return false;
    }

    skipWhitespace();

    if (
        position >= message.length() ||
        message[position++] != '}'
    ) {
        return false;
    }

    skipWhitespace();

    return position == message.length();
}

void ServerClient::sendTemperatureMeasurement(
    float beerTemperature,
    float ambientTemperature
)
{
    if (
        !_connected ||
        !_registered ||
        _webSocketClient == nullptr
    ) {
        return;
    }

    String message;

    message.reserve(180);

    message += "{\"type\":\"TEMPERATURE_MEASUREMENT\",";
    message += "\"deviceId\":\"";
    message += _deviceIdentity.getDeviceId();
    message += "\",";
    message += "\"beerTemperature\":";
    message += String(
        beerTemperature,
        1
    );
    message += ",";
    message += "\"ambientTemperature\":";
    message += String(
        ambientTemperature,
        1
    );
    message += "}";

    if (
        sendTextMessage(
            message,
            "temperature measurement"
        )
    ) {
        Serial.print(
            "ServerClient: temperature measurement sent: "
        );

        Serial.println(
            message
        );
    }
}
