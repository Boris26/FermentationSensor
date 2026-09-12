#pragma once

#include "config/SensorConfigService.h"

class PressureSensor;
class TemperatureTransmissionPolicy;

class RuntimeSensorConfigTarget : public SensorConfigTarget
{
public:
    RuntimeSensorConfigTarget(
        PressureSensor& pressureSensor,
        TemperatureTransmissionPolicy& temperatureTransmissionPolicy
    );

    void applySensorConfig(const SensorConfig& config) override;

private:
    PressureSensor& _pressureSensor;
    TemperatureTransmissionPolicy& _temperatureTransmissionPolicy;
};
