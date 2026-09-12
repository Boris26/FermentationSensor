#include "app/RuntimeSensorConfigTarget.h"

#include "network/TemperatureTransmissionPolicy.h"
#include "sensors/PressureSensor.h"

RuntimeSensorConfigTarget::RuntimeSensorConfigTarget(
    PressureSensor& pressureSensor,
    TemperatureTransmissionPolicy& temperatureTransmissionPolicy
) :
    _pressureSensor(pressureSensor),
    _temperatureTransmissionPolicy(temperatureTransmissionPolicy)
{
}

void RuntimeSensorConfigTarget::applySensorConfig(const SensorConfig& config)
{
    _pressureSensor.applyConfig(config);
    _temperatureTransmissionPolicy.setSendDeltaC(config.temperatureSendDeltaC);
}
