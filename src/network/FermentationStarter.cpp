#include "network/FermentationStarter.h"

#include "network/GatewayEndpoint.h"
#include "network/ServerClient.h"

FermentationStarter::FermentationStarter(ServerClient& serverClient)
    : _serverClient(serverClient)
{
}

void FermentationStarter::requestStart()
{
    // Pause/resume and a later reconnect must not create another business start.
    if (_requested) return;
    _requested = true;
    _nextAttemptMs = millis();
}

void FermentationStarter::update()
{
    if (!_requested || _completed || !_serverClient.isRegistered()) return;

    const String& beerId = _serverClient.finishedBeerId();
    if (!isSafePathSegment(beerId)) return;

    const unsigned long now = millis();
    if (static_cast<long>(now - _nextAttemptMs) < 0) return;

    if (postStart(beerId)) {
        _completed = true;
        Serial.println("FermentationStarter: fermentation started.");
        return;
    }

    // Use the same bounded exponential retry policy as the gateway connection.
    _nextAttemptMs = now + _retryIntervalMs;
    _retryIntervalMs *= 2;
    if (_retryIntervalMs > MAX_RETRY_INTERVAL_MS) {
        _retryIntervalMs = MAX_RETRY_INTERVAL_MS;
    }
}

bool FermentationStarter::postStart(const String& beerId)
{
    const GatewayEndpoint& endpoint = _serverClient.endpoint();
    if (!endpoint.isValid()) return false;

    HttpClient client(_wifiClient, endpoint.address.c_str(), endpoint.port);
    const String path = String("/finishedbeer/") + beerId +
        "/start-fermentation";

    // Do not use the body overload: BeerDataStore owns timestamp and state.
    const int requestResult = client.post(path);
    if (requestResult != 0) {
        Serial.print("FermentationStarter: POST failed, result=");
        Serial.println(requestResult);
        client.stop();
        return false;
    }

    const int status = client.responseStatusCode();
    client.stop();
    if (status >= 200 && status < 300) return true;

    Serial.print("FermentationStarter: BeerDataStore status=");
    Serial.println(status);
    return false;
}

bool FermentationStarter::isSafePathSegment(const String& value)
{
    if (value.isEmpty()) return false;
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}
