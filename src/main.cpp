#include <Arduino.h>
#include <cstring>
#include <kv_config.h>
#include <kvstore_global_api.h>

#include "config/Config.h"

#include "device/DeviceIdentity.h"

#include "input/MeasurementButton.h"

#include "network/GatewayDiscovery.h"
#include "network/MeasurementOutbox.h"
#include "network/MeasurementSequenceAllocator.h"
#include "network/NetworkManager.h"
#include "network/ServerClient.h"
#include "network/TemperatureTransmissionPolicy.h"
#include "network/WifiCredentials.h"
#include "network/WifiSetupPortal.h"

#include "output/ErrorLed.h"
#include "output/StatusLed.h"

#include "sensors/PressureSensor.h"
#include "sensors/TemperatureSensor.h"

#include "session/MeasurementSession.h"

#include "storage/FlashStorage.h"
#include "storage/GatewayEndpointStore.h"
#include "storage/MeasurementSequenceStore.h"
#include "storage/StorageMaintenance.h"
#include "storage/TemperatureSensorStore.h"
#include "storage/WifiCredentialStore.h"


FlashStorage flashStorage;
StorageMaintenance storageMaintenance(flashStorage);


DeviceIdentity deviceIdentity(
    flashStorage
);


NetworkManager networkManager;


WifiCredentialStore wifiCredentialStore(
    flashStorage
);

WifiSetupPortal wifiSetupPortal(
    wifiCredentialStore
);


GatewayEndpointStore gatewayEndpointStore(
    flashStorage
);

GatewayDiscovery gatewayDiscovery;

ServerClient serverClient(
    deviceIdentity
);

TemperatureTransmissionPolicy temperatureTransmissionPolicy(
    TEMPERATURE_SEND_DELTA_C
);

MeasurementSequenceStore measurementSequenceStore(
    flashStorage
);

MeasurementSequenceAllocator measurementSequenceAllocator(
    measurementSequenceStore
);

MeasurementOutbox measurementOutbox(
    MEASUREMENT_ACK_TIMEOUT_MS,
    measurementSequenceAllocator
);


PressureSensor pressureSensor;


TemperatureSensorStore temperatureSensorStore(
    flashStorage
);

TemperatureSensor temperatureSensor(
    ONE_WIRE_PIN,
    temperatureSensorStore
);


MeasurementButton measurementButton(
    MEASUREMENT_BUTTON_PIN
);


MeasurementSession measurementSession;


StatusLed statusLed(
    STATUS_LED_PIN
);

StatusLed sessionLed(
    SESSION_LED_PIN
);

ErrorLed errorLed(
    SENSOR_ERROR_LED_PIN
);


MeasurementState lastMeasurementState =
    MeasurementState::IDLE;


bool sensorsReady = false;

bool sessionInitialized = false;

bool gatewayEndpointActive = false;

bool cachedEndpointPending = false;

bool wifiWasConnected = false;

bool discoveryAttemptActive = false;

bool serverWasRegistered = false;

bool measurementSequenceReady = false;

constexpr char STORAGE_MAINTENANCE_COMMAND[] = "COMPACT_STORAGE";
char storageMaintenanceCommandBuffer[sizeof(STORAGE_MAINTENANCE_COMMAND)] = {};
size_t storageMaintenanceCommandLength = 0;
bool storageMaintenanceCommandOverflow = false;

unsigned long nextDiscoveryAttemptMs = 0;


unsigned long lastSensorCheckMs = 0;

void resetRuntimeMeasurementState()
{
    measurementOutbox.resetRuntimeState();
    temperatureTransmissionPolicy.resetRuntimeState();
}

bool factoryReset()
{
    return flashStorage.factoryReset();
}

void announceStorageMaintenanceAvailable()
{
    Serial.println("STORAGE_MAINTENANCE_AVAILABLE");
    Serial.println("Send COMPACT_STORAGE at any time while storage error is active");
}

void resetStorageMaintenanceCommandBuffer()
{
    storageMaintenanceCommandLength = 0;
    storageMaintenanceCommandBuffer[0] = '\0';
    storageMaintenanceCommandOverflow = false;
}

void updateStorageMaintenanceCommand()
{
    if (measurementSequenceReady) return;

    while (Serial.available()) {
        const char character = static_cast<char>(Serial.read());

        if (character == '\r') continue;

        if (character != '\n') {
            if (storageMaintenanceCommandLength <
                sizeof(storageMaintenanceCommandBuffer) - 1) {
                storageMaintenanceCommandBuffer[
                    storageMaintenanceCommandLength++
                ] = character;
                storageMaintenanceCommandBuffer[
                    storageMaintenanceCommandLength
                ] = '\0';
            }
            else {
                storageMaintenanceCommandOverflow = true;
            }
            continue;
        }

        const bool commandMatches =
            !storageMaintenanceCommandOverflow &&
            strcmp(
                storageMaintenanceCommandBuffer,
                STORAGE_MAINTENANCE_COMMAND
            ) == 0;

        resetStorageMaintenanceCommandBuffer();

        if (!commandMatches) continue;

        if (storageMaintenance.compactPersistentStorage()) {
            Serial.println("STORAGE_MAINTENANCE_RESTARTING");
            Serial.flush();
            delay(50);
            NVIC_SystemReset();
        }

        return;
    }
}


enum class ErrorState
{
    NONE,
    STORAGE,
    SENSOR,
    NETWORK,
    BACKEND
};


ErrorState lastErrorState =
    ErrorState::NONE;


const char* measurementTypeName(MeasurementType type)
{
    return type == MeasurementType::TEMPERATURE
        ? "TEMPERATURE"
        : "BUBBLE_ACTIVITY";
}


void printOutboxOverflowIfChanged(uint32_t previousDroppedCount)
{
    if (measurementOutbox.droppedCount() == previousDroppedCount) return;

    Serial.print("MEASUREMENT_OUTBOX_OVERFLOW,");
    Serial.print(measurementTypeName(measurementOutbox.lastDroppedType()));
    Serial.print(',');
    Serial.print(measurementOutbox.lastDroppedSequence());
    Serial.print(',');
    Serial.println(measurementOutbox.droppedCount());
}


void printMeasurementBuffered(const OutboxEntry& entry)
{
    Serial.print("MEASUREMENT_BUFFERED,");
    Serial.print(measurementTypeName(entry.type));
    Serial.print(',');
    Serial.print(entry.sequence);
    Serial.print(',');
    Serial.println(measurementOutbox.size());
}


void updateMeasurementLeds()
{
    const MeasurementState state =
        measurementSession.getState();


    switch (state) {

        case MeasurementState::IDLE:
        {
            statusLed.on();

            sessionLed.off();

            break;
        }


        case MeasurementState::RUNNING:
        {
            statusLed.off();

            sessionLed.startBlinking(
                SESSION_LED_BLINK_INTERVAL_MS
            );

            break;
        }


        case MeasurementState::PAUSED:
        {
            statusLed.startBlinking(
                STATUS_LED_BLINK_INTERVAL_MS
            );

            sessionLed.startBlinking(
                SESSION_LED_BLINK_INTERVAL_MS
            );

            break;
        }
    }
}


void updateErrorLed()
{
    ErrorState currentErrorState =
        ErrorState::NONE;


    // Durable sequence reservation is required before measurements can be
    // queued and therefore has the highest priority.
    if (!measurementSequenceReady) {
        currentErrorState =
            ErrorState::STORAGE;
    }
    else if (!sensorsReady) {
        currentErrorState =
            ErrorState::SENSOR;
    }
    else if (!networkManager.isConnected()) {
        currentErrorState =
            ErrorState::NETWORK;
    }
    else if (
        !serverClient.isConnected() ||
        !serverClient.isRegistered()
    ) {
        currentErrorState =
            ErrorState::BACKEND;
    }


    // Do not restart the blink timer
    // on every loop iteration.
    if (
        currentErrorState ==
        lastErrorState
    ) {
        return;
    }


    lastErrorState =
        currentErrorState;


    switch (currentErrorState) {

        case ErrorState::NONE:
        {
            errorLed.off();

            break;
        }


        case ErrorState::STORAGE:
        {
            // A steady error LED makes the boot-time persistent-storage
            // failure visible even while networking and sensors keep running.
            errorLed.on();

            break;
        }


        case ErrorState::SENSOR:
        {
            errorLed.startBlinking(
                ERROR_LED_BLINK_INTERVAL_MS
            );

            break;
        }


        case ErrorState::NETWORK:
        {
            errorLed.startBlinking(
                NETWORK_ERROR_LED_BLINK_INTERVAL_MS
            );

            break;
        }


        case ErrorState::BACKEND:
        {
            errorLed.startBlinking(
                BACKEND_ERROR_LED_BLINK_INTERVAL_MS
            );

            break;
        }
    }
}


void initializeSessionIfSensorsReady()
{
    const bool sensorsOk =
        temperatureSensor
            .areAllSensorsConnected();


    if (!sensorsOk) {
        sensorsReady = false;

        statusLed.off();

        sessionLed.off();


        Serial.println(
            "Temperature sensors not ready."
        );


        return;
    }


    sensorsReady = true;


    if (!sessionInitialized) {
        measurementSession.begin();

        sessionInitialized = true;


        lastMeasurementState =
            measurementSession.getState();


        updateMeasurementLeds();


        Serial.println(
            "Temperature sensors ready."
        );

        Serial.println(
            "Measurement session initialized."
        );
    }
}


void updateSensorInitialization()
{
    if (temperatureSensor.isConversionInProgress()) {
        return;
    }

    const unsigned long now =
        millis();


    if (
        now - lastSensorCheckMs <
        SENSOR_CHECK_INTERVAL_MS
    ) {
        return;
    }


    lastSensorCheckMs =
        now;


    temperatureSensor.refresh();


    initializeSessionIfSensorsReady();
}


void updateServerClient()
{
    const bool wifiConnected = networkManager.isConnected();

    if (!wifiConnected) {
        if (wifiWasConnected) {
            serverClient.onNetworkDisconnected();
            serverClient.stop();
            gatewayDiscovery.stop();
            gatewayEndpointActive = false;
            cachedEndpointPending = false;
            discoveryAttemptActive = false;
        }
        wifiWasConnected = false;
        serverWasRegistered = false;
        return;
    }

    if (!wifiWasConnected) {
        wifiWasConnected = true;
        const GatewayEndpoint cached = gatewayEndpointStore.load();
        if (cached.isValid()) {
            Serial.println("Gateway: trying last-known endpoint cache first.");
            serverClient.begin(cached);
            gatewayEndpointActive = true;
            cachedEndpointPending = true;
        } else {
            gatewayDiscovery.start();
            discoveryAttemptActive = gatewayDiscovery.isRunning();
            if (!discoveryAttemptActive) {
                nextDiscoveryAttemptMs = millis() + GATEWAY_DISCOVERY_RETRY_INTERVAL_MS;
            }
        }
    }

    if (gatewayEndpointActive) {
        serverClient.update();

        const bool serverRegistered =
            serverClient.isRegistered();

        if (
            serverRegistered &&
            !serverWasRegistered
        ) {
            temperatureTransmissionPolicy
                .requestCurrentMeasurement();
        }

        serverWasRegistered =
            serverRegistered;

        if (serverClient.isRegistered()) cachedEndpointPending = false;

        const uint8_t failureLimit = cachedEndpointPending
            ? 1
            : GATEWAY_REDISCOVERY_FAILURE_THRESHOLD;
        if (serverClient.failedConnectionCycles() >= failureLimit) {
            Serial.println("Gateway: endpoint failed; starting rediscovery.");
            serverClient.stop();
            serverWasRegistered = false;
            gatewayEndpointActive = false;
            cachedEndpointPending = false;
            gatewayDiscovery.start();
            discoveryAttemptActive = gatewayDiscovery.isRunning();
            if (!discoveryAttemptActive) {
                nextDiscoveryAttemptMs = millis() + GATEWAY_DISCOVERY_RETRY_INTERVAL_MS;
            }
        }
    }

    gatewayDiscovery.update();
    GatewayEndpoint discovered;
    if (gatewayDiscovery.takeResult(discovered)) {
        gatewayEndpointStore.save(discovered);
        serverClient.begin(discovered);
        gatewayEndpointActive = true;
        cachedEndpointPending = false;
        discoveryAttemptActive = false;
        return;
    }

    if (discoveryAttemptActive && !gatewayDiscovery.isRunning()) {
        discoveryAttemptActive = false;
        nextDiscoveryAttemptMs = millis() + GATEWAY_DISCOVERY_RETRY_INTERVAL_MS;
    }

    if (!gatewayEndpointActive && !gatewayDiscovery.isRunning() &&
        !discoveryAttemptActive &&
        static_cast<long>(millis() - nextDiscoveryAttemptMs) >= 0) {
        gatewayDiscovery.start();
        discoveryAttemptActive = gatewayDiscovery.isRunning();
        if (!discoveryAttemptActive) {
            nextDiscoveryAttemptMs = millis() + GATEWAY_DISCOVERY_RETRY_INTERVAL_MS;
        }
    }
}

void updateMeasurementOutbox()
{
    uint32_t acknowledgedSequence = 0;
    if (serverClient.takeMeasurementAcknowledgement(
        acknowledgedSequence
    )) {
        if (measurementOutbox.acknowledge(
            acknowledgedSequence
        )) {
            Serial.print("MEASUREMENT_ACK,");
            Serial.print(acknowledgedSequence);
            Serial.print(',');
            Serial.println(measurementOutbox.size());
        } else {
            Serial.print("MEASUREMENT_ACK_IGNORED,");
            Serial.println(acknowledgedSequence);
        }
    }

    if (
        measurementSequenceReady &&
        pressureSensor.hasCompletedBubbleActivityWindow()
    ) {
        const uint32_t previousDroppedCount = measurementOutbox.droppedCount();
        if (measurementOutbox.enqueueBubbleActivity(
            pressureSensor.completedBubbleActivityWindow()
        )) {
            pressureSensor.acknowledgeCompletedBubbleActivityWindow();
            printOutboxOverflowIfChanged(previousDroppedCount);
            printMeasurementBuffered(measurementOutbox.back());
        }
    }

    const bool connected = serverClient.isConnected();
    const bool registered = serverClient.isRegistered();
    if (!connected || !registered) {
        measurementOutbox.onTransportUnavailable();
        return;
    }

    const unsigned long now = millis();
    if (!measurementOutbox.shouldSend(
        connected,
        registered,
        now
    )) return;

    const OutboxEntry& pending = measurementOutbox.front();
    const bool retry = measurementOutbox.isRetry();
    if (serverClient.sendMeasurement(pending, static_cast<uint32_t>(now))) {
        measurementOutbox.recordSuccessfulSend(now);
        Serial.print(retry ? "MEASUREMENT_RETRY," : "MEASUREMENT_SENT,");
        Serial.print(measurementTypeName(pending.type));
        Serial.print(',');
        Serial.print(pending.sequence);
        Serial.print(',');
        Serial.println(pending.ageSeconds(static_cast<uint32_t>(now)));
    }
}


void setup()
{
    Serial.begin(
        SERIAL_BAUD_RATE
    );


    const unsigned long serialWaitStart =
        millis();


    while (
        !Serial &&
        millis() - serialWaitStart < 3000
    ) {
        delay(10);
    }


    mbed::bd_addr_t startAddress = 0;

    mbed::bd_size_t size = 0;


    const int result =
        kv_get_default_flash_addresses(
            &startAddress,
            &size
        );


    Serial.print(
        "KV flash result: "
    );

    Serial.println(
        result
    );


    Serial.print(
        "KV start address: 0x"
    );

    Serial.println(
        static_cast<unsigned long>(
            startAddress
        ),
        HEX
    );


    Serial.print(
        "KV size: "
    );

    Serial.println(
        static_cast<unsigned long>(
            size
        )
    );


    Serial.println();


    Serial.print(
        DEVICE_ID
    );

    Serial.println(
        " starting..."
    );


    // LEDs
    statusLed.begin();

    sessionLed.begin();

    errorLed.begin();


    // Initialization indication
    statusLed.startBlinking(
        INIT_LED_BLINK_INTERVAL_MS
    );

    sessionLed.off();

    errorLed.off();


    // Persistent flash storage
    flashStorage.begin();


    // Device identity
    deviceIdentity.begin();

    // Reserve measurement sequences before any outbox enqueue is possible.
    measurementSequenceStore.begin();
    measurementSequenceReady =
        measurementSequenceAllocator.begin();

    if (!measurementSequenceReady) {
        Serial.println("MEASUREMENT_SEQUENCE_INITIALIZATION_FAILED");
        errorLed.on();
        announceStorageMaintenanceAvailable();
    }


    // Persistent configuration stores
    wifiCredentialStore.begin();

    temperatureSensorStore.begin();

    gatewayEndpointStore.begin();


    // Sensors
    pressureSensor.begin();

    temperatureSensor.begin();


    // Debug persistent configuration
    flashStorage.debugPrintAll();


    // WiFi
    const WifiCredentials storedCredentials =
        wifiCredentialStore.load();


    if (storedCredentials.isValid()) {
        Serial.print(
            "Stored SSID: "
        );

        Serial.println(
            storedCredentials.ssid
        );


        networkManager.begin(
            storedCredentials
        );
    }
    else {
        Serial.println(
            "No valid WiFi credentials stored."
        );


        wifiSetupPortal.begin();
    }


    // Button
    measurementButton.begin();


    // Session initialization is intentionally
    // performed later in loop().
    //
    // This allows the initialization LED
    // to become visible.
    Serial.print(
        DEVICE_ID
    );

    Serial.println(
        " setup complete."
    );
}


void loop()
{
    // Persistent storage maintenance is available at any time while the
    // sequence allocator is fail-closed. An accepted command performs the
    // maintenance synchronously and restarts the controller on success.
    updateStorageMaintenanceCommand();


    // Input
    measurementButton.update();


    // Network
    networkManager.update();

    wifiSetupPortal.update();

    updateServerClient();


    // Temperature sensor
    const bool newTemperatureMeasurement =
        temperatureSensor.update();


    // Initialize the measurement session
    // once the required sensors are ready.
    updateSensorInitialization();


    // The send policy runs before the generic outbox. Network availability is
    // deliberately irrelevant here: eligible measurements can wait offline.
    if (
        measurementSequenceReady &&
        newTemperatureMeasurement &&
        sessionInitialized &&
        sensorsReady &&
        measurementSession.isRunning() &&
        temperatureTransmissionPolicy.shouldSend(
            temperatureSensor.getBeerTemperature(),
            temperatureSensor.getAmbientTemperature()
        )
    ) {
        const float beerTemperature =
            temperatureSensor.getBeerTemperature();

        const float ambientTemperature =
            temperatureSensor.getAmbientTemperature();

        const bool equivalentReconnectSnapshot =
            temperatureTransmissionPolicy.isCurrentMeasurementRequested() &&
            measurementOutbox.backIsEquivalentTemperature(
                beerTemperature,
                ambientTemperature
            );

        if (equivalentReconnectSnapshot) {
            temperatureTransmissionPolicy.recordQueuedMeasurement(
                beerTemperature,
                ambientTemperature
            );
        } else {
            const uint32_t previousDroppedCount = measurementOutbox.droppedCount();
            if (measurementOutbox.enqueueTemperature(
                beerTemperature,
                ambientTemperature,
                pressureSensor.isAvailable(),
                pressureSensor.getPressurePa(),
                static_cast<uint32_t>(millis())
            )) {
                temperatureTransmissionPolicy.recordQueuedMeasurement(
                    beerTemperature,
                    ambientTemperature
                );
                printOutboxOverflowIfChanged(previousDroppedCount);
                printMeasurementBuffered(measurementOutbox.back());
            }
        }
    }


    // Update error state.
    //
    // Sensor error:
    // fast blinking
    //
    // WiFi unavailable:
    // slower blinking
    //
    // Backend unavailable or not registered:
    // slowest blinking
    //
    // Everything OK:
    // LED off
    updateErrorLed();


    // LEDs
    statusLed.update();

    sessionLed.update();

    errorLed.update();


    // Button is only active after
    // successful session initialization.
    if (
        sessionInitialized &&
        measurementButton.wasPressed()
    ) {
        measurementSession.handleButtonPress();


        const MeasurementState currentState =
            measurementSession.getState();

        if (currentState == MeasurementState::RUNNING) {
            pressureSensor.onSessionRunning();
        } else if (currentState == MeasurementState::PAUSED) {
            pressureSensor.onSessionPaused();
        }


        if (
            currentState !=
            lastMeasurementState
        ) {
            updateMeasurementLeds();


            lastMeasurementState =
                currentState;
        }
    }


    // Pressure measurement only while
    // the measurement session is running.
    if (
        sessionInitialized &&
        measurementSession.isRunning()
    ) {
        pressureSensor.update();
    }

    updateMeasurementOutbox();
}
