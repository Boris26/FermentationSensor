#pragma once

#include "app/GatewayConnectionManager.h"
#include "app/MeasurementTransport.h"
#include "app/RuntimeSensorConfigTarget.h"
#include "app/SensorSessionCoordinator.h"
#include "app/StatusController.h"
#include "app/StorageMaintenanceConsole.h"
#include "config/SensorConfigService.h"
#include "device/DeviceIdentity.h"
#include "input/MeasurementButton.h"
#include "network/ConfigHttpServer.h"
#include "network/GatewayDiscovery.h"
#include "network/MeasurementOutbox.h"
#include "network/MeasurementSequenceAllocator.h"
#include "network/NetworkManager.h"
#include "network/ServerClient.h"
#include "network/TemperatureTransmissionPolicy.h"
#include "network/WifiSetupPortal.h"
#include "output/ErrorLed.h"
#include "output/StatusLed.h"
#include "sensors/PressureSensor.h"
#include "sensors/TemperatureSensor.h"
#include "session/MeasurementSession.h"
#include "storage/FlashStorage.h"
#include "storage/GatewayEndpointStore.h"
#include "storage/MeasurementSequenceStore.h"
#include "storage/SensorConfigStore.h"
#include "storage/StorageMaintenance.h"
#include "storage/TemperatureSensorStore.h"
#include "storage/WifiCredentialStore.h"

class FermentationSensorApplication
{
public:
    FermentationSensorApplication();
    void begin();
    void update();
    void resetRuntimeMeasurementState();
    bool factoryReset();

private:
    void printBootDiagnostics();

    FlashStorage _flashStorage;
    StorageMaintenance _storageMaintenance;
    DeviceIdentity _deviceIdentity;
    NetworkManager _networkManager;
    WifiCredentialStore _wifiCredentialStore;
    WifiSetupPortal _wifiSetupPortal;
    GatewayEndpointStore _gatewayEndpointStore;
    GatewayDiscovery _gatewayDiscovery;
    ServerClient _serverClient;
    TemperatureTransmissionPolicy _temperaturePolicy;
    MeasurementSequenceStore _measurementSequenceStore;
    MeasurementSequenceAllocator _measurementSequenceAllocator;
    MeasurementOutbox _measurementOutbox;
    PressureSensor _pressureSensor;
    RuntimeSensorConfigTarget _runtimeConfigTarget;
    SensorConfigStore _sensorConfigStore;
    SensorConfigService _sensorConfigService;
    TemperatureSensorStore _temperatureSensorStore;
    TemperatureSensor _temperatureSensor;
    MeasurementButton _measurementButton;
    MeasurementSession _measurementSession;
    ConfigHttpServer _configHttpServer;
    StatusLed _statusLed;
    StatusLed _sessionLed;
    ErrorLed _errorLed;
    GatewayConnectionManager _gatewayConnection;
    MeasurementTransport _measurementTransport;
    StatusController _statusController;
    SensorSessionCoordinator _sensorSession;
    StorageMaintenanceConsole _maintenanceConsole;
    bool _measurementSequenceReady = false;
};
