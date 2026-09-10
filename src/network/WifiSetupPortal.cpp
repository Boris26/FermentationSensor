#include "network/WifiSetupPortal.h"

WifiSetupPortal::WifiSetupPortal(WifiCredentialStore& credentialStore)
    : _server(80), _credentialStore(credentialStore)
{
}

void WifiSetupPortal::begin()
{
    Serial.println("WifiSetupPortal: starting access point...");
    const int status = WiFi.beginAP("FERM-01-Setup");

    if (status != WL_AP_LISTENING) {
        Serial.println("WifiSetupPortal: failed to start access point.");
        return;
    }

    _server.begin();
    _active = true;
    Serial.println("WifiSetupPortal: access point started.");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("WifiSetupPortal: web server started.");
}

void WifiSetupPortal::update()
{
    if (!_active) return;

    if (!_clientActive) {
        _client = _server.available();
        if (!_client) return;
        _clientActive = true;
        _clientStartedMs = millis();
        Serial.println("WifiSetupPortal: client connected.");
    }

    if (!_client.connected()) {
        resetClient();
        return;
    }

    if (millis() - _clientStartedMs >= CLIENT_TIMEOUT_MS) {
        Serial.println("WifiSetupPortal: request timed out.");
        resetClient();
        return;
    }

    handleClient(_client);
}

bool WifiSetupPortal::isActive() const
{
    return _active;
}

void WifiSetupPortal::handleClient(WiFiClient& client)
{
    size_t bytesRead = 0;

    while (client.available() && bytesRead < READ_BUDGET_BYTES) {
        const char character = static_cast<char>(client.read());
        ++bytesRead;

        if (_readingBody) {
            _body += character;

            if (_body.length() > static_cast<unsigned int>(_contentLength)) {
                sendBadRequest(client);
                resetClient();
                return;
            }

            if (_body.length() == static_cast<unsigned int>(_contentLength)) {
                String ssid;
                String password;

                if (
                    !getFormParameter(_body, "ssid", ssid) ||
                    !getFormParameter(_body, "password", password)
                ) {
                    Serial.println("WifiSetupPortal: invalid form data.");
                    sendBadRequest(client);
                    resetClient();
                    return;
                }

                WifiCredentials credentials;
                credentials.ssid = ssid;
                credentials.password = password;

                if (credentials.isValid() && _credentialStore.save(credentials)) {
                    Serial.print("WifiSetupPortal: credentials stored for ");
                    Serial.println(credentials.ssid);
                    sendSuccessPage(client);
                }
                else {
                    Serial.println("WifiSetupPortal: failed to store credentials.");
                    sendSetupPage(client);
                }

                resetClient();
                return;
            }

            continue;
        }

        if (character == '\r') continue;

        if (character != '\n') {
            _headerLine += character;
            if (_headerLine.length() > 512) {
                Serial.println("WifiSetupPortal: request header too long.");
                sendBadRequest(client);
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
            if (_requestLine == "GET / HTTP/1.1") {
                sendSetupPage(client);
                resetClient();
                return;
            }

            if (
                _requestLine != "POST / HTTP/1.1" ||
                !_formContentType ||
                _contentLength <= 0 ||
                _contentLength > 512
            ) {
                sendBadRequest(client);
                resetClient();
                return;
            }

            _body.reserve(_contentLength);
            _readingBody = true;
            continue;
        }

        if (_headerLine.startsWith("Content-Length:")) {
            String value = _headerLine.substring(strlen("Content-Length:"));
            value.trim();
            for (size_t index = 0; index < value.length(); ++index) {
                if (!isDigit(value[index])) {
                    _contentLength = -1;
                    break;
                }
            }
            if (_contentLength != -1) _contentLength = value.toInt();
        }
        else if (_headerLine.startsWith("Content-Type:")) {
            String value = _headerLine.substring(strlen("Content-Type:"));
            value.trim();
            _formContentType = value == "application/x-www-form-urlencoded";
        }

        _headerLine = "";
    }
}

void WifiSetupPortal::resetClient()
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
    _formContentType = false;
    Serial.println("WifiSetupPortal: client disconnected.");
}

bool WifiSetupPortal::getFormParameter(
    const String& body,
    const String& name,
    String& value
) const
{
    int start = 0;

    while (start <= body.length()) {
        int end = body.indexOf('&', start);
        if (end < 0) end = body.length();
        const int separator = body.indexOf('=', start);

        if (separator >= start && separator < end) {
            const String encodedName = body.substring(start, separator);
            String decodedName;

            if (!urlDecode(encodedName, decodedName)) return false;

            if (decodedName == name) {
                return urlDecode(body.substring(separator + 1, end), value);
            }
        }

        if (end == body.length()) break;
        start = end + 1;
    }

    return false;
}

bool WifiSetupPortal::urlDecode(const String& value, String& decoded) const
{
    decoded = "";

    const auto hexValue = [](char character) -> int {
        if (character >= '0' && character <= '9') return character - '0';
        if (character >= 'a' && character <= 'f') return character - 'a' + 10;
        if (character >= 'A' && character <= 'F') return character - 'A' + 10;
        return -1;
    };

    for (size_t index = 0; index < value.length(); ++index) {
        if (value[index] == '+') {
            decoded += ' ';
        }
        else if (value[index] == '%') {
            if (index + 2 >= value.length()) return false;
            const int high = hexValue(value[index + 1]);
            const int low = hexValue(value[index + 2]);
            if (high < 0 || low < 0) return false;
            decoded += static_cast<char>((high << 4) | low);
            index += 2;
        }
        else {
            decoded += value[index];
        }
    }

    return true;
}

void WifiSetupPortal::sendSetupPage(WiFiClient& client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Connection: close");
    client.println();
    client.println(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>FERM-01 WLAN Setup</title></head><body>"
        "<h1>FERM-01 WLAN Setup</h1><p>Bitte WLAN-Zugangsdaten eingeben.</p>"
        "<form method=\"POST\" action=\"/\">"
        "<label for=\"ssid\">WLAN Name</label><br>"
        "<input type=\"text\" id=\"ssid\" name=\"ssid\" required><br><br>"
        "<label for=\"password\">Passwort</label><br>"
        "<input type=\"password\" id=\"password\" name=\"password\"><br><br>"
        "<button type=\"submit\">Speichern</button></form></body></html>"
    );
}

void WifiSetupPortal::sendSuccessPage(WiFiClient& client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Connection: close");
    client.println();
    client.println(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>WLAN gespeichert</title></head><body><h1>WLAN gespeichert</h1>"
        "<p>Die WLAN-Zugangsdaten wurden gespeichert.</p></body></html>"
    );
}

void WifiSetupPortal::sendBadRequest(WiFiClient& client)
{
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Content-Type: text/plain; charset=utf-8");
    client.println("Connection: close");
    client.println();
    client.println("Ungueltige Anfrage.");
}
