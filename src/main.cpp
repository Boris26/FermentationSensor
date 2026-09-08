#include <Arduino.h>

#include "config/Config.h"
#include "sensors/PressureSensor.h"
#include "network/NetworkManager.h" 
#include "network/WifiCredentialStore.h"
#include "network/WifiCredentials.h"
#include "network/WifiSetupPortal.h"
#include "output/StatusLed.h"
#include "input/PauseButton.h"

NetworkManager networkManager;
PressureSensor pressureSensor;
PauseButton pauseButton(PAUSE_BUTTON_PIN);

WifiCredentialStore wifiCredentialStore;
WifiSetupPortal wifiSetupPortal(wifiCredentialStore);

StatusLed statusLed(STATUS_LED_PIN);

bool lastPauseState = false;

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    const unsigned long serialWaitStart = millis();

    while (!Serial && millis() - serialWaitStart < 3000) {
        delay(10);
    }

    Serial.println();
    Serial.print(DEVICE_ID);
    Serial.println(" starting...");

    wifiCredentialStore.begin();

    WifiCredentials storedCredentials =
        wifiCredentialStore.load();

    if (storedCredentials.isValid()) {
    Serial.print("Stored SSID: ");
    Serial.println(storedCredentials.ssid);

    networkManager.begin(storedCredentials);
}
else {
    Serial.println("No valid WiFi credentials stored.");
    wifiSetupPortal.begin();
}

    pauseButton.begin();
    pressureSensor.begin();
    statusLed.begin();

    Serial.print(DEVICE_ID);
    Serial.println(" ready.");
}

void loop()
{
    pauseButton.update();
    networkManager.update();
    wifiSetupPortal.update();
    statusLed.update();

    const bool paused = pauseButton.isPaused();

    if (paused != lastPauseState) {
        if (paused) {
            Serial.println("Measurement paused.");
            statusLed.startBlinking(500);
        }
        else {
            Serial.println("Measurement resumed.");
            statusLed.off();
        }

        lastPauseState = paused;
    }

    if (!paused) {
        pressureSensor.update();
    }
}