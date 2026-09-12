#pragma once

class GatewayDiscovery;
class GatewayEndpointStore;
class NetworkManager;
class ServerClient;
class TemperatureTransmissionPolicy;

class GatewayConnectionManager
{
public:
    GatewayConnectionManager(
        NetworkManager& networkManager,
        GatewayEndpointStore& endpointStore,
        GatewayDiscovery& discovery,
        ServerClient& serverClient,
        TemperatureTransmissionPolicy& temperaturePolicy
    );

    void update();

private:
    void startDiscovery();

    NetworkManager& _networkManager;
    GatewayEndpointStore& _endpointStore;
    GatewayDiscovery& _discovery;
    ServerClient& _serverClient;
    TemperatureTransmissionPolicy& _temperaturePolicy;
    bool _endpointActive = false;
    bool _cachedEndpointPending = false;
    bool _wifiWasConnected = false;
    bool _discoveryAttemptActive = false;
    bool _serverWasRegistered = false;
    unsigned long _nextDiscoveryAttemptMs = 0;
};
