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
    _recoveryState = RecoveryState::DISCOVERY;
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
        _recoveryState = RecoveryState::WIFI_RECONNECT;
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
            _recoveryState = RecoveryState::SOCKET_RECONNECT;
        } else {
            startDiscovery();
        }
    }

    if (_endpointActive) {
        _serverClient.update();
        const bool registered = _serverClient.isRegistered();
        if (registered && !_serverWasRegistered) {
            _temperaturePolicy.requestCurrentMeasurement();
            _recoveryState = RecoveryState::CONNECTED;
        }
        _serverWasRegistered = registered;
        if (registered) _cachedEndpointPending = false;

        const uint8_t failures = _serverClient.failedConnectionCycles();
        if (_cachedEndpointPending && failures >= 1) {
            Serial.println("Gateway: endpoint failed; starting rediscovery.");
            _serverClient.stop();
            _serverWasRegistered = false;
            _endpointActive = false;
            _cachedEndpointPending = false;
            startDiscovery();
        } else if (failures >= WIFI_RECOVERY_FAILURE_THRESHOLD) {
            Serial.println("Gateway: repeated TCP failures; escalating to WiFi reset.");
            _serverClient.stop();
            _endpointActive = false;
            _wifiWasConnected = false;
            _networkManager.requestReconnect("backend unreachable after socket resets");
            _recoveryState = RecoveryState::WIFI_RECONNECT;
            return;
        } else if (failures > 0) {
            _recoveryState = RecoveryState::SOCKET_RECONNECT;
        }
    }

    _discovery.update();
    GatewayEndpoint discovered;
    if (_discovery.takeResult(discovered)) {
        _endpointStore.save(discovered);
        _serverClient.begin(discovered);
        _endpointActive = true;
        _cachedEndpointPending = false;
        _recoveryState = RecoveryState::SOCKET_RECONNECT;
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
