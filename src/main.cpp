#include <Arduino.h>

#include "config/Config.h"

#include "input/PauseButton.h"

#include "network/NetworkManager.h"
#include "network/WifiCredentialStore.h"
#include "network/WifiCredentials.h"
#include "network/WifiSetupPortal.h"

#include "output/ErrorLed.h"
#include "output/StatusLed.h"

#include "sensors/PressureSensor.h"
#include "sensors/TemperatureSensor.h"
#include "sensors/TemperatureSensorStore.h"


NetworkManager networkManager;

PressureSensor pressureSensor;

PauseButton pauseButton(
    PAUSE_BUTTON_PIN
);

TemperatureSensorStore temperatureSensorStore;

TemperatureSensor temperatureSensor(
    ONE_WIRE_PIN,
    temperatureSensorStore
);

WifiCredentialStore wifiCredentialStore;

WifiSetupPortal wifiSetupPortal(
    wifiCredentialStore
);

StatusLed statusLed(
    STATUS_LED_PIN
);

ErrorLed errorLed(
    SENSOR_ERROR_LED_PIN
);


bool lastPauseState = false;

bool lastSensorErrorState = false;

unsigned long lastSensorCheckMs = 0;

constexpr unsigned long SENSOR_CHECK_INTERVAL_MS = 1000;


void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

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


    // Ein-/Ausgänge
    pauseButton.begin();

    statusLed.begin();

    errorLed.begin();


    // Sensoren
    pressureSensor.begin();

    temperatureSensorStore.begin();

    temperatureSensor.begin();


    // Initialen Temperatursensor-Status prüfen
    if (
        temperatureSensor.areAllSensorsConnected()
    ) {
        errorLed.off();
        lastSensorErrorState = false;
    }
    else {
        Serial.println(
            "Temperature sensor error."
        );

        errorLed.startBlinking(500);
        lastSensorErrorState = true;
    }


    Serial.print(DEVICE_ID);
    Serial.println(" ready.");
}


void loop()
{
    pauseButton.update();

    networkManager.update();

    wifiSetupPortal.update();

    statusLed.update();

    errorLed.update();

    temperatureSensor.update();


    // Pause-Taster auswerten
    const bool paused =
        pauseButton.isPaused();

    if (paused != lastPauseState) {
        if (paused) {
            Serial.println(
                "Measurement paused."
            );

            statusLed.startBlinking(500);
        }
        else {
            Serial.println(
                "Measurement resumed."
            );

            statusLed.off();
        }

        lastPauseState = paused;
    }


    // Druckmessung nur wenn nicht pausiert
    if (!paused) {
        pressureSensor.update();
    }


    // Temperatursensoren regelmäßig prüfen
    const unsigned long now =
        millis();

    if (
        now - lastSensorCheckMs >=
        SENSOR_CHECK_INTERVAL_MS
    ) {
        lastSensorCheckMs = now;

        const bool sensorError =
            !temperatureSensor
                .areAllSensorsConnected();

        if (
            sensorError !=
            lastSensorErrorState
        ) {
            if (sensorError) {
                Serial.println(
                    "Temperature sensor error."
                );

                errorLed.startBlinking(
                    500
                );
            }
            else {
                Serial.println(
                    "Temperature sensors OK."
                );

                errorLed.off();
            }

            lastSensorErrorState =
                sensorError;
        }
    }
}