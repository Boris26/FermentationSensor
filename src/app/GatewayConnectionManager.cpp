#include <Arduino.h>

#include "app/GatewayConnectionManager.h"
#include "config/Config.h"
#include "network/GatewayDiscovery.h"
#include "network/NetworkManager.h"
#include "network/ServerClient.h"
#include "network/TemperatureTransmissionPolicy.h"
#include "storage/GatewayEndpointStore.h"

GatewayConnectionManager::GatewayConnectionManager(
    NetworkManager& networkManager,
    GatewayEndpointStore& endpointStore,
    GatewayDiscovery& discovery,
    ServerClient& serverClient,
    TemperatureTransmissionPolicy& temperaturePolicy
) : _networkManager(networkManager), _endpointStore(endpointStore),
    _discovery(discovery), _serverClient(serverClient),
    _temperaturePolicy(temperaturePolicy)
{
}

void GatewayConnectionManager::startDiscovery()
{
    _discovery.start();
    _discoveryAttemptActive = _discovery.isRunning();
    if (!_discoveryAttemptActive) {
        _nextDiscoveryAttemptMs = millis() + GATEWAY_DISCOVERY_RETRY_INTERVAL_MS;
    }
}

void GatewayConnectionManager::update()
{
    const bool wifiConnected = _networkManager.isConnected();
    if (!wifiConnected) {
        if (_wifiWasConnected) {
            _serverClient.onNetworkDisconnected();
            _serverClient.stop();
            _discovery.stop();
            _endpointActive = false;
            _cachedEndpointPending = false;
            _discoveryAttemptActive = false;
        }
        _wifiWasConnected = false;
        _serverWasRegistered = false;
        return;
    }

    if (!_wifiWasConnected) {
        _wifiWasConnected = true;
        const GatewayEndpoint cached = _endpointStore.load();
        if (cached.isValid()) {
            Serial.println("Gateway: trying last-known endpoint cache first.");
            _serverClient.begin(cached);
            _endpointActive = true;
            _cachedEndpointPending = true;
        } else {
            startDiscovery();
        }
    }

    if (_endpointActive) {
        _serverClient.update();
        const bool registered = _serverClient.isRegistered();
        if (registered && !_serverWasRegistered) {
            _temperaturePolicy.requestCurrentMeasurement();
        }
        _serverWasRegistered = registered;
        if (registered) _cachedEndpointPending = false;

        const uint8_t failureLimit = _cachedEndpointPending
            ? 1 : GATEWAY_REDISCOVERY_FAILURE_THRESHOLD;
        if (_serverClient.failedConnectionCycles() >= failureLimit) {
            Serial.println("Gateway: endpoint failed; starting rediscovery.");
            _serverClient.stop();
            _serverWasRegistered = false;
            _endpointActive = false;
            _cachedEndpointPending = false;
            startDiscovery();
        }
    }

    _discovery.update();
    GatewayEndpoint discovered;
    if (_discovery.takeResult(discovered)) {
        _endpointStore.save(discovered);
        _serverClient.begin(discovered);
        _endpointActive = true;
        _cachedEndpointPending = false;
        _discoveryAttemptActive = false;
        return;
    }
    if (_discoveryAttemptActive && !_discovery.isRunning()) {
        _discoveryAttemptActive = false;
        _nextDiscoveryAttemptMs = millis() + GATEWAY_DISCOVERY_RETRY_INTERVAL_MS;
    }
    if (!_endpointActive && !_discovery.isRunning() &&
        !_discoveryAttemptActive &&
        static_cast<long>(millis() - _nextDiscoveryAttemptMs) >= 0) {
        startDiscovery();
    }
}
