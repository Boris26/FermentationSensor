#include <Arduino.h>

#include "app/FermentationSensorApplication.h"
#include "config/Config.h"

namespace
{
constexpr unsigned long SLOW_LOOP_DETAIL_LOG_INTERVAL_MS = 60000;
}

FermentationSensorApplication::FermentationSensorApplication() :
    _storageMaintenance(_flashStorage),
    _deviceIdentity(_flashStorage),
    _wifiCredentialStore(_flashStorage),
    _wifiSetupPortal(_wifiCredentialStore),
    _gatewayEndpointStore(_flashStorage),
    _serverClient(_deviceIdentity),
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
        _statusController, _serverClient
    ),
    _maintenanceConsole(_storageMaintenance)
{
}

void FermentationSensorApplication::printBootDiagnostics()
{
    Serial.println("Target: Arduino Nano ESP32 (ESP32-S3), persistence: NVS");
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
    const unsigned long loopStart = millis();
    unsigned long partStart = loopStart;
    _maintenanceConsole.update(_measurementSequenceReady);
    const unsigned long maintenanceMs = millis() - partStart;
    partStart = millis();
    _sensorSession.updateInput();
    const unsigned long inputMs = millis() - partStart;
    partStart = millis();
    _networkManager.update();
    const unsigned long wifiMs = millis() - partStart;
    partStart = millis();
    _wifiSetupPortal.update();
    const unsigned long portalMs = millis() - partStart;
    partStart = millis();
    _gatewayConnection.update();
    const unsigned long socketMs = millis() - partStart;
    partStart = millis();
    _configHttpServer.update();
    const unsigned long configServerMs = millis() - partStart;
    partStart = millis();
    _sensorSession.updateMeasurements(_measurementSequenceReady);
    const unsigned long temperatureMs = millis() - partStart;
    partStart = millis();
    _statusController.update(
        _measurementSequenceReady, _sensorSession.sensorsReady()
    );
    const unsigned long statusMs = millis() - partStart;
    partStart = millis();
    _sensorSession.updateSessionInput();
    const unsigned long sessionAndPressureMs = millis() - partStart;
    partStart = millis();
    _measurementTransport.update(_measurementSequenceReady);
    const unsigned long transportMs = millis() - partStart;
    const unsigned long loopDuration = millis() - loopStart;

    static unsigned long lastModerateSlowLoopLogMs = 0;
    const unsigned long now = millis();
    const bool severeSlowLoop = loopDuration > 500;
    const bool moderateSlowLoopDue =
        loopDuration > 100 &&
        (
            lastModerateSlowLoopLogMs == 0 ||
            now - lastModerateSlowLoopLogMs >= SLOW_LOOP_DETAIL_LOG_INTERVAL_MS
        );

    if (severeSlowLoop || moderateSlowLoopDue) {
        if (!severeSlowLoop) {
            lastModerateSlowLoopLogMs = now;
        }

        Serial.print('['); Serial.print(now);
        Serial.print(" ms] SLOW_LOOP duration="); Serial.print(loopDuration);
        Serial.print(" ms severity=");
        Serial.println(loopDuration > 1000 ? ">1000ms" :
            (loopDuration > 500 ? ">500ms" : ">100ms"));
        Serial.print("  MaintenanceConsole.update: "); Serial.print(maintenanceMs); Serial.println(" ms");
        Serial.print("  MeasurementButton.update: "); Serial.print(inputMs); Serial.println(" ms");
        Serial.print("  NetworkManager.update: "); Serial.print(wifiMs); Serial.println(" ms");
        Serial.print("  WifiSetupPortal.update: "); Serial.print(portalMs); Serial.println(" ms");
        Serial.print("  ServerClient/Gateway.update: "); Serial.print(socketMs); Serial.println(" ms");
        Serial.print("  ConfigHttpServer.update: "); Serial.print(configServerMs); Serial.println(" ms");
        Serial.print("  TemperatureSensor.update: "); Serial.print(temperatureMs); Serial.println(" ms");
        Serial.print("  StatusController.update: "); Serial.print(statusMs); Serial.println(" ms");
        Serial.print("  Session/PressureSensor.update: "); Serial.print(sessionAndPressureMs); Serial.println(" ms");
        Serial.print("  MeasurementTransport.update: "); Serial.print(transportMs); Serial.println(" ms");
    }
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
