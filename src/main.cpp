#include <Arduino.h>
#include <kv_config.h>
#include <kvstore_global_api.h>

#include "config/Config.h"

#include "device/DeviceIdentity.h"

#include "input/MeasurementButton.h"

#include "network/BootstrapServer.h"
#include "network/DiscoveryService.h"
#include "network/NetworkManager.h"
#include "network/ServerClient.h"
#include "network/WifiCredentials.h"
#include "network/WifiSetupPortal.h"

#include "output/ErrorLed.h"
#include "output/StatusLed.h"

#include "sensors/PressureSensor.h"
#include "sensors/TemperatureSensor.h"

#include "session/MeasurementSession.h"

#include "storage/FlashStorage.h"
#include "storage/ServerConfigurationStore.h"
#include "storage/TemperatureSensorStore.h"
#include "storage/WifiCredentialStore.h"


FlashStorage flashStorage;


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


DiscoveryService discoveryService(
    deviceIdentity
);


ServerConfigurationStore serverConfigurationStore(
    flashStorage
);

BootstrapServer bootstrapServer(
    serverConfigurationStore
);

ServerClient serverClient(
    deviceIdentity
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

bool serverClientStarted = false;


unsigned long lastSensorCheckMs = 0;


enum class ErrorState
{
    NONE,
    SENSOR,
    NETWORK
};


ErrorState lastErrorState =
    ErrorState::NONE;


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


    // Sensor error has the highest priority.
    if (!sensorsReady) {
        currentErrorState =
            ErrorState::SENSOR;
    }
    else if (!networkManager.isConnected()) {
        currentErrorState =
            ErrorState::NETWORK;
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
    if (sessionInitialized) {
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
    if (!networkManager.isConnected()) {
        return;
    }


    if (!serverClientStarted) {
        Serial.println(
            "ServerClient: checking stored server configuration..."
        );


        const ServerConfiguration configuration =
            serverConfigurationStore.load();


        Serial.print(
            "ServerClient: host="
        );

        Serial.println(
            configuration.host
        );


        Serial.print(
            "ServerClient: port="
        );

        Serial.println(
            configuration.port
        );


        Serial.print(
            "ServerClient: path="
        );

        Serial.println(
            configuration.path
        );


        if (configuration.isValid()) {
            Serial.println(
                "ServerClient: configuration valid."
            );


            serverClient.begin(
                configuration
            );


            serverClientStarted = true;
        }
        else {
            Serial.println(
                "ServerClient: no valid server configuration."
            );


            // Prevent flooding the serial output.
            delay(1000);
        }
    }


    serverClient.update();
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


    // Persistent configuration stores
    wifiCredentialStore.begin();

    temperatureSensorStore.begin();

    serverConfigurationStore.begin();


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
    // Input
    measurementButton.update();


    // Network
    networkManager.update();

    wifiSetupPortal.update();

    discoveryService.update();

    bootstrapServer.update();

    updateServerClient();


    // Temperature sensor
    temperatureSensor.update();


    // Initialize the measurement session
    // once the required sensors are ready.
    updateSensorInitialization();


    // Update error state.
    //
    // Sensor error:
    // fast blinking
    //
    // WiFi unavailable:
    // slower blinking
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
}