#include <Arduino.h>

#include "config/Config.h"

#include "input/MeasurementButton.h"

#include "network/NetworkManager.h"
#include "network/WifiCredentialStore.h"
#include "network/WifiCredentials.h"
#include "network/WifiSetupPortal.h"

#include "output/ErrorLed.h"
#include "output/StatusLed.h"

#include "sensors/PressureSensor.h"
#include "sensors/TemperatureSensor.h"
#include "sensors/TemperatureSensorStore.h"

#include "session/MeasurementSession.h"


NetworkManager networkManager;

WifiCredentialStore wifiCredentialStore;

WifiSetupPortal wifiSetupPortal(
    wifiCredentialStore
);


PressureSensor pressureSensor;

TemperatureSensorStore temperatureSensorStore;

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

unsigned long lastSensorCheckMs = 0;


void updateMeasurementLeds()
{
    const MeasurementState state =
        measurementSession.getState();

    switch (state) {
        case MeasurementState::IDLE:
            statusLed.on();
            sessionLed.off();
            break;

        case MeasurementState::RUNNING:
            statusLed.off();

            sessionLed.startBlinking(
                SESSION_LED_BLINK_INTERVAL_MS
            );
            break;

        case MeasurementState::PAUSED:
            statusLed.startBlinking(
                STATUS_LED_BLINK_INTERVAL_MS
            );

            sessionLed.startBlinking(
                SESSION_LED_BLINK_INTERVAL_MS
            );
            break;
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

        errorLed.startBlinking(
            ERROR_LED_BLINK_INTERVAL_MS
        );

        Serial.println(
            "Temperature sensors not ready."
        );

        return;
    }


    sensorsReady = true;

    errorLed.off();


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

    lastSensorCheckMs = now;

    temperatureSensor.refresh();

    initializeSessionIfSensorsReady();
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

    Serial.println();

    Serial.print(DEVICE_ID);
    Serial.println(" starting...");


    // WLAN
    wifiCredentialStore.begin();

    WifiCredentials storedCredentials =
        wifiCredentialStore.load();

    if (storedCredentials.isValid()) {
        Serial.print("Stored SSID: ");

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


    // LEDs
    statusLed.begin();
    sessionLed.begin();
    errorLed.begin();


    // Sensoren
    pressureSensor.begin();

    temperatureSensorStore.begin();

    temperatureSensor.begin();


    // Erst Sensoren prüfen.
    // Session wird nur bei gültigen Sensoren gestartet.
    initializeSessionIfSensorsReady();


    Serial.print(DEVICE_ID);
    Serial.println(" ready.");
}


void loop()
{
    // Eingaben
    measurementButton.update();


    // Netzwerk
    networkManager.update();

    wifiSetupPortal.update();


    // LEDs
    statusLed.update();
    sessionLed.update();
    errorLed.update();


    // Temperatursensor läuft unabhängig von der Session weiter
    temperatureSensor.update();


    // Falls Sensoren beim Start fehlten:
    // regelmäßig erneut versuchen
    updateSensorInitialization();


    // Button nur verwenden,
    // wenn die Session erfolgreich initialisiert wurde
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


    // Druckmessung nur bei laufender Session
    if (
        sessionInitialized &&
        measurementSession.isRunning()
    ) {
        pressureSensor.update();
    }
}