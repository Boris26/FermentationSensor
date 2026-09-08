#include "network/WifiSetupPortal.h"

WifiSetupPortal::WifiSetupPortal(
    WifiCredentialStore& credentialStore
)
    : _server(80),
      _credentialStore(credentialStore)
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
    if (!_active) {
        return;
    }

    WiFiClient client = _server.available();

    if (!client) {
        return;
    }

    handleClient(client);
}

bool WifiSetupPortal::isActive() const
{
    return _active;
}

void WifiSetupPortal::handleClient(WiFiClient& client)
{
    Serial.println("WifiSetupPortal: client connected.");

    const unsigned long timeoutStart = millis();

    while (
        client.connected() &&
        millis() - timeoutStart < 1000
    ) {
        if (!client.available()) {
            continue;
        }

        const String requestLine =
            client.readStringUntil('\r');

        Serial.print("WifiSetupPortal: request: ");
        Serial.println(requestLine);

        // Restliche HTTP-Header verwerfen
        while (client.available()) {
            client.read();
        }

        if (requestLine.startsWith("GET /?")) {
            const String ssid =
                getQueryParameter(requestLine, "ssid");

            const String password =
                getQueryParameter(requestLine, "password");

            WifiCredentials credentials;
            credentials.ssid = urlDecode(ssid);
            credentials.password = urlDecode(password);

            if (
                credentials.isValid() &&
                _credentialStore.save(credentials)
            ) {
                Serial.print(
                    "WifiSetupPortal: credentials stored for "
                );
                Serial.println(credentials.ssid);

                sendSuccessPage(client);
            }
            else {
                Serial.println(
                    "WifiSetupPortal: failed to store credentials."
                );

                sendSetupPage(client);
            }
        }
        else {
            sendSetupPage(client);
        }

        break;
    }

    delay(1);
    client.stop();

    Serial.println("WifiSetupPortal: client disconnected.");
}

String WifiSetupPortal::getQueryParameter(
    const String& request,
    const String& name
)
{
    const String search = name + "=";

    const int start = request.indexOf(search);

    if (start < 0) {
        return "";
    }

    const int valueStart =
        start + search.length();

    int valueEnd =
        request.indexOf('&', valueStart);

    if (valueEnd < 0) {
        valueEnd =
            request.indexOf(' ', valueStart);
    }

    if (valueEnd < 0) {
        valueEnd = request.length();
    }

    return request.substring(
        valueStart,
        valueEnd
    );
}

String WifiSetupPortal::urlDecode(
    const String& value
)
{
    String result;

    for (
        unsigned int i = 0;
        i < value.length();
        ++i
    ) {
        if (value[i] == '+') {
            result += ' ';
            continue;
        }

        if (
            value[i] == '%' &&
            i + 2 < value.length()
        ) {
            const String hex =
                value.substring(
                    i + 1,
                    i + 3
                );

            const char decoded =
                static_cast<char>(
                    strtol(
                        hex.c_str(),
                        nullptr,
                        16
                    )
                );

            result += decoded;
            i += 2;

            continue;
        }

        result += value[i];
    }

    return result;
}

void WifiSetupPortal::sendSetupPage(
    WiFiClient& client
)
{
    client.println("HTTP/1.1 200 OK");
    client.println(
        "Content-Type: text/html; charset=utf-8"
    );
    client.println("Connection: close");
    client.println();

    client.println(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"utf-8\">"
        "<meta name=\"viewport\" "
        "content=\"width=device-width, initial-scale=1\">"
        "<title>FERM-01 WLAN Setup</title>"
        "</head>"
        "<body>"

        "<h1>FERM-01 WLAN Setup</h1>"
        "<p>Bitte WLAN-Zugangsdaten eingeben.</p>"

        "<form method=\"GET\" action=\"/\">"

        "<label for=\"ssid\">WLAN Name</label><br>"
        "<input type=\"text\" "
        "id=\"ssid\" "
        "name=\"ssid\" "
        "required><br><br>"

        "<label for=\"password\">Passwort</label><br>"
        "<input type=\"password\" "
        "id=\"password\" "
        "name=\"password\"><br><br>"

        "<button type=\"submit\">"
        "Speichern"
        "</button>"

        "</form>"

        "</body>"
        "</html>"
    );
}

void WifiSetupPortal::sendSuccessPage(
    WiFiClient& client
)
{
    client.println("HTTP/1.1 200 OK");
    client.println(
        "Content-Type: text/html; charset=utf-8"
    );
    client.println("Connection: close");
    client.println();

    client.println(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"utf-8\">"
        "<meta name=\"viewport\" "
        "content=\"width=device-width, initial-scale=1\">"
        "<title>WLAN gespeichert</title>"
        "</head>"
        "<body>"

        "<h1>WLAN gespeichert</h1>"
        "<p>"
        "Die WLAN-Zugangsdaten wurden gespeichert."
        "</p>"

        "</body>"
        "</html>"
    );
}