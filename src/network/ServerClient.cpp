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
            RECONNECT_INTERVAL_MS
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
}


bool ServerClient::isConnected() const
{
    return _connected;
}


bool ServerClient::isRegistered() const
{
    return _registered;
}


void ServerClient::connect()
{
    if (!_configured) {
        return;
    }


    _lastConnectionAttemptMs =
        millis();


    if (_webSocketClient != nullptr) {
        _webSocketClient->stop();

        delete _webSocketClient;

        _webSocketClient =
            nullptr;
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


    _webSocketClient =
        new WebSocketClient(
            _wifiClient,
            _configuration.host.c_str(),
            _configuration.port
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

        delete _webSocketClient;

        _webSocketClient =
            nullptr;


        _connected = false;

        _registered = false;

        return;
    }


    _connected = true;

    _registered = false;


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

        delete _webSocketClient;

        _webSocketClient =
            nullptr;
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

    _webSocketClient->beginMessage(
        TYPE_TEXT
    );

    _webSocketClient->write(
        reinterpret_cast<const uint8_t*>(
            message.c_str()
        ),
        message.length()
    );

    const int result =
        _webSocketClient->endMessage();

    if (result == 0) {
        Serial.print(
            "ServerClient: registration sent: "
        );

        Serial.println(
            message
        );
    }
    else {
        Serial.print(
            "ServerClient: failed to send registration, result="
        );

        Serial.println(
            result
        );
    }
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
    if (
        message.indexOf(
            "\"type\":\"ACK\""
        ) >= 0
    ) {
        if (!_registered) {
            _registered = true;


            Serial.println(
                "ServerClient: sensor registered."
            );
        }


        return;
    }


    Serial.println(
        "ServerClient: unknown message."
    );
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

    _webSocketClient->beginMessage(
        TYPE_TEXT
    );

    _webSocketClient->write(
        reinterpret_cast<const uint8_t*>(
            message.c_str()
        ),
        message.length()
    );

    const int result =
        _webSocketClient->endMessage();

    if (result == 0) {
        Serial.print(
            "ServerClient: temperature measurement sent: "
        );

        Serial.println(
            message
        );
    }
    else {
        Serial.print(
            "ServerClient: failed to send temperature measurement, result="
        );

        Serial.println(
            result
        );
    }
}