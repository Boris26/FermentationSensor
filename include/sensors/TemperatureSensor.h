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
    void update();

    void refresh();
    bool areAllSensorsConnected();

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

    unsigned long _lastMeasurementMs = 0;
};