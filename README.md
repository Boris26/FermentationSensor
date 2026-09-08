# FermentationSensor

Firmware for the fermentation sensor of the Braumeister/Brauhaus system.

The FermentationSensor monitors the progress of beer fermentation and provides measurement data to the BeerDataStore.

The firmware runs on an **Arduino Nano RP2040 Connect** and is developed using **PlatformIO**.

The goal is not only to collect individual measurements. Over multiple brews, the system should build comparable fermentation profiles that may later be used to estimate when a beer is approaching the end of fermentation.

---

## Goals

During fermentation, the sensor should collect and derive the following information:

- Beer temperature
- Ambient temperature
- Differential pressure at the airlock
- Baseline differential pressure
- Number of fermentation bubbles
- Bubble frequency
- Bubble intensity
- Fermentation activity over time
- MeasurementSession status

The measurement data will later be associated with the corresponding beer and brew in the BeerDataStore.

This allows multiple fermentations of the same recipe to be compared.

---

# Hardware

## Microcontroller

The project uses an:

**Arduino Nano RP2040 Connect**

The Nano RP2040 Connect uses an RP2040 as its main processor and a NINA-W102 module for Wi-Fi communication.

---

## Temperature Sensors

Two waterproof **DS18B20** sensors are used:

- Beer temperature
- Ambient temperature

Both sensors share the same 1-Wire bus.

### Wiring

| Function | Connection |
|---|---|
| Power | 3.3 V |
| Ground | GND |
| 1-Wire DATA | D4 |
| Pull-up | 4.7 kΩ between DATA and 3.3 V |

Only one shared 4.7 kΩ pull-up resistor is required for both sensors.

### Sensor Roles

The sensors are distinguished by their unique DS18B20 ROM IDs.

The assignments are stored persistently.

During initial setup, sensors can be learned automatically.

If the ambient sensor is already known and exactly one new sensor appears, that sensor can automatically be assigned as the beer sensor.

The assignment remains stored even if a sensor is temporarily disconnected.

---

# Differential Pressure and Fermentation Activity

The planned pressure sensor is:

**DFRobot SEN0343 / Fermion LWLP5000 ±500 Pa**

Communication is performed via **I²C**.

The purpose of the sensor is not simply to measure the absolute pressure inside the fermentation vessel.

Instead, it observes the **differential pressure between the airlock side and ambient pressure**.

This allows both slow pressure changes and fast pressure events caused by individual fermentation bubbles to be observed.

---

## Pressure Sensor Wiring

Planned connection to the Nano RP2040 Connect:

| Pressure Sensor | Nano RP2040 Connect |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SDA | A4 / SDA |
| SCL | A5 / SCL |

The sensor is read via I²C.

---

## Mechanical Installation

The electronics are mounted on top of the fermentation vessel.

The pressure sensor should be positioned slightly **above the airlock**.

A short hose connects the airlock to the pressure sensor.

The hose should run downward from the sensor toward the airlock.

```text
Pressure Sensor
      |
      | short hose
      |
      v
   Airlock
      |
      v
Fermentation Vessel
```

This arrangement should:

- minimize damping of fast pressure changes
- keep the air volume between the airlock and sensor small
- allow condensation to drain back toward the airlock
- provide reproducible measurement conditions between different brews

The hose type, length, and geometry should remain as consistent as possible between fermentations.

---

# Pressure Signal Analysis

The differential pressure signal is expected to contain several different types of information.

These should be analyzed separately.

## Baseline Differential Pressure

During strong fermentation, a sustained differential pressure may develop at the airlock.

Mechanically, this can sometimes be observed because the moving part of the airlock remains continuously lifted.

This slowly changing component of the pressure signal will be treated as the **baseline pressure**.

As fermentation activity decreases, this baseline pressure may also decrease.

Example:

```text
Differential Pressure
     ^
     |
     |                ___________
     |             __/           \__
     |          __/                 \__
     |_________/
     |
     +----------------------------------> Time

            Baseline pressure rises
                       and later decreases
```

Baseline pressure should therefore be recorded independently from individual bubble events.

---

## Bubble Detection

Fast pressure changes caused by individual fermentation bubbles are superimposed on the baseline pressure.

Simplified example:

```text
Differential Pressure
     ^
     |
     |            /\       /\       /\
     |       ____/  \_____/  \_____/  \____
     |______/
     |
     +-------------------------------------> Time

          Baseline pressure

                ↑        ↑        ↑
              Bubble   Bubble   Bubble
```

The pressure sensor must be sampled frequently enough to detect these events.

At minimum, the following information should eventually be derived:

- Time of a bubble event
- Bubble count
- Bubbles per minute
- Bubble intensity
- Average bubble intensity
- Maximum bubble intensity

The exact metrics will be determined after analyzing real measurements.

---

# Bubble Intensity

The number of bubbles alone may not be sufficient to describe fermentation activity.

The strength of the pressure change produced by an individual bubble may also contain useful information.

For example, two periods could have the same bubble frequency:

```text
10 bubbles/min + strong pressure pulses

10 bubbles/min + weak pressure pulses
```

Although the frequency is identical, the underlying fermentation activity may be different.

Bubble frequency and bubble intensity should therefore be recorded separately.

A possible future representation could be:

```json
{
  "bubblesPerMinute": 9.6,
  "bubbleIntensityAvgPa": 31.4,
  "bubbleIntensityMaxPa": 46.8
}
```

The final definition of bubble intensity will only be established after practical measurements with the real sensor.

---

# Fermentation Activity

In the long term, several measurements should be considered together:

```text
Beer Temperature
       +
Ambient Temperature
       +
Baseline Differential Pressure
       +
Bubble Frequency
       +
Bubble Intensity
       +
Manual Plato Measurements
       |
       v
Fermentation Activity Profile
```

The goal is not only to determine **whether fermentation is still producing bubbles**, but also how the activity changes over time.

A possible fermentation profile might look like:

```text
Early fermentation
    |
    +-- increasing baseline pressure
    +-- increasing bubble frequency
    +-- increasing bubble intensity

Active fermentation
    |
    +-- sustained baseline pressure
    +-- high bubble frequency
    +-- strong bubble intensity

Declining fermentation
    |
    +-- baseline pressure decreases
    +-- bubbles may still occur frequently
    +-- bubble intensity decreases

Late fermentation
    |
    +-- low baseline pressure
    +-- few bubbles
    +-- weak bubble intensity
```

This interpretation must be validated using real fermentation data.

---

# Comparing Multiple Brews

An important long-term goal is to compare repeated brews of the same recipe.

The BeerDataStore can associate sensor data with the corresponding beer and recipe.

Over multiple brewing sessions, this creates a historical dataset.

```text
Brew 1 ─┐
Brew 2 ─┤
Brew 3 ─┼──> Typical fermentation
Brew 4 ─┤      profile for recipe
Brew 5 ─┘
```

Potential comparison values include:

- Time since fermentation started
- Temperature profile
- Baseline differential pressure
- Bubble frequency
- Bubble intensity
- Plato measurements
- Yeast used
- Confirmed fermentation completion time

This can later be used to determine whether characteristic fermentation profiles exist for individual recipes.

---

# Approaching the End of Fermentation

The FermentationSensor itself should not make a definitive decision such as:

> "The beer has finished fermenting."

Instead, historical data should eventually provide indications that fermentation is approaching its typical end state.

For example, the backend could determine that the current brew is approaching the end range of previous brews of the same recipe based on:

- temperature
- baseline differential pressure
- bubble frequency
- bubble intensity
- time since fermentation started

The application could then recommend:

> Fermentation activity is approaching the typical end range for this recipe. A Plato measurement is recommended.

A Plato measurement can then be used as an additional confirmation.

---

# Future Data Analysis

Historical fermentation data should be stored in a way that allows more advanced analysis later.

Possible applications include:

- comparison of multiple brews
- detection of typical fermentation profiles
- detection of unusual fermentation behavior
- statistical models
- recipe-specific predictions
- machine-learning / AI models

For this reason, relevant measurements should not be reduced to a single "fermentation activity" number too early.

Where possible, the following information should remain separately available:

- temperature
- baseline differential pressure
- bubble frequency
- bubble intensity
- timing information

This allows future analysis to determine which features are actually useful for evaluating fermentation progress.

---

# Measurement Button

The button is connected to **D3** and uses `INPUT_PULLUP`.

Therefore:

```text
Open        -> HIGH
Button/GND  -> LOW
```

The button controls the local `MeasurementSession`.

## States

```text
IDLE
  |
  | Button
  v
RUNNING
  |
  | Button
  v
PAUSED
  |
  | Button
  v
RUNNING
```

An explicit `STOP` state is currently not implemented.

---

# LEDs

The firmware uses separate LEDs for operating status, MeasurementSession state, and sensor errors.

| LED | Pin | Purpose |
|---|---:|---|
| Status | D2 | General session/operating state |
| Sensor Error | D5 | Missing or unavailable expected sensors |
| Session | D6 | Active MeasurementSession |

## IDLE

```text
Status LED   -> continuously on
Session LED  -> off
Error LED    -> off
```

## RUNNING

```text
Status LED   -> off
Session LED  -> blinking
Error LED    -> off
```

## PAUSED

```text
Status LED   -> blinking
Session LED  -> blinking
Error LED    -> off
```

## Sensor Error

```text
Status LED   -> off
Session LED  -> off
Error LED    -> blinking
```

---

# Startup Behavior

When the device starts, the required sensors are checked first.

A `MeasurementSession` is initialized only when the required sensors are available.

Currently this check applies to the temperature sensors. Once the pressure sensor is integrated, it should also become part of the startup sensor check.

```text
Power On
   |
   v
Initialize Sensors
   |
   v
Check Sensors
   |
   +---- OK ----------------------+
   |                              |
   |                              v
   |                     Initialize
   |                   MeasurementSession
   |                              |
   |                              v
   |                            IDLE
   |
   +---- NOT OK
          |
          v
     Error LED blinks
          |
          v
     Retry sensor check
       periodically
          |
          v
     Sensors available?
          |
          +---- No --> retry
          |
          +---- Yes
                 |
                 v
           Error LED off
                 |
                 v
             Initialize
          MeasurementSession
                 |
                 v
               IDLE
```

While required sensors are unavailable:

- the firmware continues running
- Wi-Fi remains active
- sensors are checked again periodically
- the MeasurementSession is not initialized
- the Measurement Button has no effect
- the red error LED blinks

---

# Temperature Measurement

Both DS18B20 sensors are monitored independently of the MeasurementSession state.

For storage in the BeerDataStore, temperatures should later be represented as **whole degrees Celsius**.

Example:

```text
20.31 °C -> 20 °C
20.62 °C -> 21 °C
```

Internally, the full DS18B20 resolution can still be used.

A new temperature value should only be transmitted or stored when the rounded value changes compared with the previously transmitted value.

A maximum transmission interval may additionally be introduced later.

---

# Wi-Fi

Wi-Fi credentials are stored persistently.

At startup, the sensor automatically attempts to connect to the stored Wi-Fi network.

If no valid credentials are available, the sensor starts its own access point:

```text
FERM-01-Setup
```

The local setup page allows the user to configure:

- SSID
- Wi-Fi password

The credentials are stored persistently.

A temporary Wi-Fi outage does not automatically start the setup portal.

Instead, the `NetworkManager` periodically attempts to reconnect to the known network.

---

# Device ID

Each FermentationSensor has a stable device ID.

Currently:

```cpp
constexpr char DEVICE_ID[] = "FERM-01";
```

The BeerDataStore will later use this ID to associate the physical device with an active fermentation.

The sensor itself does not need to know a `beer_id`.

The mapping

```text
Device -> Beer
```

is managed by the backend.

---

# Network Architecture

Wi-Fi connectivity and BeerDataStore communication are intentionally separated.

```text
NetworkManager
      |
      +-- Wi-Fi
      +-- Reconnect
      +-- Connection status


ServerClient
      |
      +-- BeerDataStore
      +-- Device status
      +-- Measurements
      +-- Commands / Socket
```

The `NetworkManager` is responsible only for network connectivity.

The planned `ServerClient` will handle communication with the BeerDataStore.

The backend API and socket protocol will first be defined in the BeerDataStore and then implemented in the firmware.

---

# System Architecture

The FermentationSensor operates as a client.

```text
FermentationSensor
       |
       v
BeerDataStore
       |
       v
Brauhaus
```

The Brauhaus frontend does not communicate directly with the Arduino.

The BeerDataStore is intended to manage:

- registered FermentationSensor devices
- device status
- Device-to-Beer assignments
- measurement data
- fermentation activity
- MeasurementSession state
- historical fermentation profiles
- backend commands
- socket communication with the Brauhaus frontend

---

# Measurement Data

The sensors do not need to operate at the same sampling frequency.

Temperature changes relatively slowly.

Differential pressure must be sampled much more frequently in order to detect and analyze individual bubble events.

```text
DS18B20
   |
   +-- slow sampling
   |
   +-- relevant temperature changes


LWLP5000
   |
   +-- fast sampling
   |
   +-- determine baseline pressure
   |
   +-- detect bubbles
   |
   +-- determine bubble intensity
   |
   +-- aggregate values over time windows
```

A future measurement record could contain:

```json
{
  "beerTemperature": 20,
  "ambientTemperature": 21,
  "baselinePressurePa": 24.6,
  "bubbleCount": 17,
  "bubblesPerMinute": 3.4,
  "bubbleIntensityAvgPa": 31.4,
  "bubbleIntensityMaxPa": 46.8
}
```

The final measurement model will be defined after practical pressure sensor testing and together with the backend API.

---

# Pressure Sensor Testing

Before using the pressure sensor with an actual beer, it should first be tested using a simple experimental setup.

The setup should resemble the final installation as closely as possible:

```text
Pressure Sensor
      |
      | short hose
      |
      v
   Airlock
      |
      v
Fermentation Vessel
     with water
```

Carefully applying pressure to the fermentation vessel or lid can create artificial pressure changes and bubble events.

For the first tests, no complex bubble detection algorithm should be used.

Instead, raw sensor readings should be printed to Serial at a relatively high sampling rate.

Example:

```text
Time [ms]    Differential Pressure [Pa]

10200        1.4
10250        2.1
10300        4.8
10350       12.7
10400       28.3
10450       41.6
10500        8.2
10550        2.7
```

The tests should help determine:

- sensor noise
- zero point
- baseline pressure
- pressure buildup before a bubble
- shape of a bubble event
- peak amplitude
- event duration
- pressure drop after a bubble
- influence of hose length
- behavior under sustained differential pressure

The final bubble detection algorithm should only be designed after analyzing these real measurements.

---

# Offline Behavior

Measurement data should not be lost during temporary Wi-Fi or BeerDataStore outages whenever possible.

Initially, a RAM-based queue is planned.

```text
Measurement
     |
     v
Backend available?
     |
     +-- Yes --> Send
     |
     +-- No --> Queue
                  |
                  v
           Connection restored
                  |
                  v
            Send queued data
```

A persistent flash-based ring buffer may be added later if measurements should also survive device restarts or power failures.

---

# Project Structure

```text
include/
├── config/
│   └── Config.h
├── input/
│   └── MeasurementButton.h
├── network/
│   ├── NetworkManager.h
│   ├── ServerClient.h
│   ├── WifiCredentialStore.h
│   ├── WifiCredentials.h
│   └── WifiSetupPortal.h
├── output/
│   ├── ErrorLed.h
│   └── StatusLed.h
├── sensors/
│   ├── PressureSensor.h
│   ├── TemperatureSensor.h
│   ├── TemperatureSensorId.h
│   └── TemperatureSensorStore.h
└── session/
    └── MeasurementSession.h

src/
├── input/
│   └── MeasurementButton.cpp
├── network/
│   ├── NetworkManager.cpp
│   ├── ServerClient.cpp
│   ├── WifiCredentialStore.cpp
│   └── WifiSetupPortal.cpp
├── output/
│   ├── ErrorLed.cpp
│   └── StatusLed.cpp
├── sensors/
│   ├── PressureSensor.cpp
│   ├── TemperatureSensor.cpp
│   └── TemperatureSensorStore.cpp
├── session/
│   └── MeasurementSession.cpp
└── main.cpp
```

---

# PlatformIO

```ini
[env:nanorp2040connect]
platform = raspberrypi
board = nanorp2040connect
framework = arduino

monitor_speed = 115200

lib_deps =
    milesburton/DallasTemperature
    paulstoffregen/OneWire
    arduino-libraries/WiFiNINA
```

---

# Current Development Status

## Implemented

- Arduino Nano RP2040 Connect
- PlatformIO project
- Wi-Fi using WiFiNINA
- persistent Wi-Fi credentials
- Wi-Fi setup portal
- automatic Wi-Fi reconnect
- two DS18B20 sensors on a shared 1-Wire bus
- persistent temperature sensor IDs
- automatic sensor role assignment
- Ambient and Beer sensor distinction
- missing temperature sensor detection
- repeated 1-Wire bus scanning
- detection of sensors reconnected at runtime
- sensor error LED
- Measurement Button
- MeasurementSession with `IDLE`, `RUNNING`, and `PAUSED`
- status LED
- dedicated session LED
- MeasurementSession initialization only after successful sensor validation

## Next Steps

- connect the SEN0343 / LWLP5000
- test I²C communication
- output raw differential pressure values
- test with water and the real airlock
- analyze baseline pressure
- analyze real bubble pressure profiles
- develop bubble detection
- determine bubble intensity

## Afterwards

- reduce temperature values to whole degrees Celsius for backend storage
- define the measurement data model
- implement an offline queue
- implement `ServerClient`
- define the BeerDataStore interface
- implement status/heartbeat communication
- implement socket communication
- compare historical fermentation profiles
- explore statistical and AI-assisted fermentation analysis