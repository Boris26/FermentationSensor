#include <Arduino.h>
#include <kv_config.h>
#include <kvstore_global_api.h>

#include "app/FermentationSensorApplication.h"
#include "config/Config.h"

FermentationSensorApplication::FermentationSensorApplication() :
    _storageMaintenance(_flashStorage),
    _deviceIdentity(_flashStorage),
    _wifiCredentialStore(_flashStorage),
    _wifiSetupPortal(_wifiCredentialStore),
    _gatewayEndpointStore(_flashStorage),
    _serverClient(_deviceIdentity),
    _fermentationStarter(_serverClient),
    _temperaturePolicy(TEMPERATURE_SEND_DELTA_C),
    _measurementSequenceStore(_flashStorage),
    _measurementSequenceAllocator(_measurementSequenceStore),
    _measurementOutbox(MEASUREMENT_ACK_TIMEOUT_MS, _measurementSequenceAllocator),
    _runtimeConfigTarget(_pressureSensor, _temperaturePolicy),
    _sensorConfigStore(_flashStorage),
    _sensorConfigService(_sensorConfigStore, _runtimeConfigTarget),
    _temperatureSensorStore(_flashStorage),
    _temperatureSensor(ONE_WIRE_PIN, _temperatureSensorStore),
    _measurementButton(MEASUREMENT_BUTTON_PIN),
    _configHttpServer(
        _sensorConfigService, _deviceIdentity, _measurementSession, _serverClient
    ),
    _statusLed(STATUS_LED_PIN),
    _sessionLed(SESSION_LED_PIN),
    _errorLed(SENSOR_ERROR_LED_PIN),
    _gatewayConnection(
        _networkManager, _gatewayEndpointStore, _gatewayDiscovery,
        _serverClient, _temperaturePolicy
    ),
    _measurementTransport(_serverClient, _measurementOutbox, _pressureSensor),
    _statusController(
        _statusLed, _sessionLed, _errorLed, _networkManager, _serverClient
    ),
    _sensorSession(
        _temperatureSensor, _pressureSensor, _measurementButton,
        _measurementSession, _temperaturePolicy, _measurementTransport,
        _fermentationStarter, _statusController
    ),
    _maintenanceConsole(_storageMaintenance)
{
}

void FermentationSensorApplication::printBootDiagnostics()
{
    mbed::bd_addr_t startAddress = 0;
    mbed::bd_size_t size = 0;
    const int result = kv_get_default_flash_addresses(&startAddress, &size);
    Serial.print("KV flash result: ");
    Serial.println(result);
    Serial.print("KV start address: 0x");
    Serial.println(static_cast<unsigned long>(startAddress), HEX);
    Serial.print("KV size: ");
    Serial.println(static_cast<unsigned long>(size));
    Serial.println();
    Serial.print(DEVICE_ID);
    Serial.println(" starting...");
}

void FermentationSensorApplication::begin()
{
    Serial.begin(SERIAL_BAUD_RATE);
    const unsigned long serialWaitStart = millis();
    while (!Serial && millis() - serialWaitStart < 3000) delay(10);
    printBootDiagnostics();
    _statusController.begin();
    _flashStorage.begin();
    _deviceIdentity.begin();
    _sensorConfigStore.begin();
    _sensorConfigService.begin();
    _measurementSequenceStore.begin();
    _measurementSequenceReady = _measurementSequenceAllocator.begin();
    if (!_measurementSequenceReady) {
        Serial.println("MEASUREMENT_SEQUENCE_INITIALIZATION_FAILED");
        _errorLed.on();
        _maintenanceConsole.announceAvailable();
    }
    _wifiCredentialStore.begin();
    _temperatureSensorStore.begin();
    _gatewayEndpointStore.begin();
    _sensorSession.begin();
    _flashStorage.debugPrintAll();

    const WifiCredentials storedCredentials = _wifiCredentialStore.load();
    if (storedCredentials.isValid()) {
        Serial.print("Stored SSID: ");
        Serial.println(storedCredentials.ssid);
        _networkManager.begin(storedCredentials);
        _configHttpServer.begin();
    } else {
        Serial.println("No valid WiFi credentials stored.");
        _wifiSetupPortal.begin();
    }
    Serial.print(DEVICE_ID);
    Serial.println(" setup complete.");
}

void FermentationSensorApplication::update()
{
    _maintenanceConsole.update(_measurementSequenceReady);
    _sensorSession.updateInput();
    _networkManager.update();
    _wifiSetupPortal.update();
    _gatewayConnection.update();
    _configHttpServer.update();
    _sensorSession.updateMeasurements(_measurementSequenceReady);
    _statusController.update(
        _measurementSequenceReady, _sensorSession.sensorsReady()
    );
    _sensorSession.updateSessionInput();
    _fermentationStarter.update();
    _measurementTransport.update(_measurementSequenceReady);
}

void FermentationSensorApplication::resetRuntimeMeasurementState()
{
    _measurementTransport.resetRuntimeState();
    _temperaturePolicy.resetRuntimeState();
}

bool FermentationSensorApplication::factoryReset()
{
    return _flashStorage.factoryReset();
}
