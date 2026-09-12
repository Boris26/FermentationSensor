#include <Arduino.h>

#include "app/MeasurementTransport.h"
#include "network/MeasurementOutbox.h"
#include "network/ServerClient.h"
#include "sensors/PressureSensor.h"

namespace
{
const char* measurementTypeName(MeasurementType type)
{
    return type == MeasurementType::TEMPERATURE
        ? "TEMPERATURE" : "BUBBLE_ACTIVITY";
}
}

MeasurementTransport::MeasurementTransport(
    ServerClient& serverClient,
    MeasurementOutbox& outbox,
    PressureSensor& pressureSensor
) : _serverClient(serverClient), _outbox(outbox), _pressureSensor(pressureSensor)
{
}

void MeasurementTransport::printOverflowIfChanged(
    uint32_t previousDroppedCount
) const
{
    if (_outbox.droppedCount() == previousDroppedCount) return;
    Serial.print("MEASUREMENT_OUTBOX_OVERFLOW,");
    Serial.print(measurementTypeName(_outbox.lastDroppedType()));
    Serial.print(',');
    Serial.print(_outbox.lastDroppedSequence());
    Serial.print(',');
    Serial.println(_outbox.droppedCount());
}

void MeasurementTransport::printBuffered() const
{
    const OutboxEntry& entry = _outbox.back();
    Serial.print("MEASUREMENT_BUFFERED,");
    Serial.print(measurementTypeName(entry.type));
    Serial.print(',');
    Serial.print(entry.sequence);
    Serial.print(',');
    Serial.println(_outbox.size());
}

bool MeasurementTransport::bufferTemperature(
    float beer, float ambient, bool pressureAvailable, float pressure,
    uint32_t measuredAtMs
)
{
    const uint32_t previousDroppedCount = _outbox.droppedCount();
    if (!_outbox.enqueueTemperature(
        beer, ambient, pressureAvailable, pressure, measuredAtMs
    )) return false;
    printOverflowIfChanged(previousDroppedCount);
    printBuffered();
    return true;
}

bool MeasurementTransport::lastTemperatureIsEquivalent(
    float beer, float ambient
) const
{
    return _outbox.backIsEquivalentTemperature(beer, ambient);
}

void MeasurementTransport::resetRuntimeState()
{
    _outbox.resetRuntimeState();
}

void MeasurementTransport::update(bool sequenceReady)
{
    uint32_t acknowledgedSequence = 0;
    if (_serverClient.takeMeasurementAcknowledgement(acknowledgedSequence)) {
        if (_outbox.acknowledge(acknowledgedSequence)) {
            Serial.print("MEASUREMENT_ACK,");
            Serial.print(acknowledgedSequence);
            Serial.print(',');
            Serial.println(_outbox.size());
        } else {
            Serial.print("MEASUREMENT_ACK_IGNORED,");
            Serial.println(acknowledgedSequence);
        }
    }

    if (sequenceReady && _pressureSensor.hasCompletedBubbleActivityWindow()) {
        const uint32_t previousDroppedCount = _outbox.droppedCount();
        if (_outbox.enqueueBubbleActivity(
            _pressureSensor.completedBubbleActivityWindow()
        )) {
            _pressureSensor.acknowledgeCompletedBubbleActivityWindow();
            printOverflowIfChanged(previousDroppedCount);
            printBuffered();
        }
    }

    const bool connected = _serverClient.isConnected();
    const bool registered = _serverClient.isRegistered();
    if (!connected || !registered) {
        _outbox.onTransportUnavailable();
        return;
    }
    const unsigned long now = millis();
    if (!_outbox.shouldSend(connected, registered, now)) return;

    const OutboxEntry& pending = _outbox.front();
    const bool retry = _outbox.isRetry();
    if (_serverClient.sendMeasurement(pending, static_cast<uint32_t>(now))) {
        _outbox.recordSuccessfulSend(now);
        Serial.print(retry ? "MEASUREMENT_RETRY," : "MEASUREMENT_SENT,");
        Serial.print(measurementTypeName(pending.type));
        Serial.print(',');
        Serial.print(pending.sequence);
        Serial.print(',');
        Serial.println(pending.ageSeconds(static_cast<uint32_t>(now)));
    }
}
