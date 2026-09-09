#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#include "sensors/TemperatureSensorId.h"
#include "storage/TemperatureSensorStore.h"

class TemperatureSensor
{
public:
    TemperatureSensor(
        uint8_t pin,
        TemperatureSensorStore& sensorStore
    );

    void begin();
    bool update();

    void refresh();
    bool areAllSensorsConnected();

    float getBeerTemperature() const;
    float getAmbientTemperature() const;

private:
    void printSensorAddress(
        const DeviceAddress& address
    );

    TemperatureSensorId toSensorId(
        const DeviceAddress& address
    );

    bool isAmbientSensor(
        const DeviceAddress& address
    );

    bool isBeerSensor(
        const DeviceAddress& address
    );

    bool isSensorConnected(
        const TemperatureSensorId& sensorId
    );

    uint8_t _pin;

    OneWire _oneWire;
    DallasTemperature _sensors;

    TemperatureSensorStore& _sensorStore;

    TemperatureSensorId _ambientSensorId;
    TemperatureSensorId _beerSensorId;

    float _ambientTemperature =
        DEVICE_DISCONNECTED_C;

    float _beerTemperature =
        DEVICE_DISCONNECTED_C;

    unsigned long _lastMeasurementMs = 0;
};