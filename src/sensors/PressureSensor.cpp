#include "sensors/PressureSensor.h"

#include <Arduino.h>
#include <DFRobot_LWLP.h>

namespace
{
    DFRobot_LWLP lwlp;

    PressureBubbleConfig defaultPressureConfig()
    {
        const SensorConfig config = SensorConfig::defaults();
        return {config.calibrationMs, config.minTriggerDeltaPa, config.noiseFactor,
            config.releaseFactor, config.minDurationMs, config.maxDurationMs,
            config.refractoryMs, config.baselineTrackingAlpha};
    }
}

PressureSensor::PressureSensor()
    : _bubbleDetector(defaultPressureConfig()),
    _bubbleActivityAggregator(SensorConfig::defaults().bubbleActivityWindowMs)
{
}

void PressureSensor::applyConfig(const SensorConfig& config)
{
    _config = config;
    _bubbleDetector.reconfigure({config.calibrationMs, config.minTriggerDeltaPa,
        config.noiseFactor, config.releaseFactor, config.minDurationMs,
        config.maxDurationMs, config.refractoryMs, config.baselineTrackingAlpha});
    _bubbleActivityAggregator.reconfigure(config.bubbleActivityWindowMs);
    _diagnosedWindowRevision = 0;
    _lastReadMs = 0;
}


void PressureSensor::begin()
{
    Serial.println(
        "PressureSensor: initializing..."
    );

    const int result =
        lwlp.begin();

    if (result != 0)
    {
        _available = false;

        Serial.print(
            "PressureSensor: initialization failed, error="
        );

        Serial.println(result);

        return;
    }

    _available = true;

    Serial.println(
        "PressureSensor: ready."
    );
}


void PressureSensor::update()
{
    if (!_available)
    {
        return;
    }

    const unsigned long now =
        millis();

    if (
        now - _lastReadMs < _config.sampleIntervalMs
    )
    {
        return;
    }

    _lastReadMs = now;

    const DFRobot_LWLP::sLwlp_t data =
        lwlp.getData();

    _pressurePa =
        data.presure;

    const bool wasCalibrated =
        _bubbleDetector.isCalibrated();

    const bool bubbleDetected =
        _bubbleDetector.processSample(now, _pressurePa);

    if (!wasCalibrated && _bubbleDetector.isCalibrated()) {
        _bubbleActivityAggregator.start(now);
    } else {
        // Advance the window before assigning an event recognized at `now`.
        // Thus a boundary event belongs only to the newly started window.
        _bubbleActivityAggregator.update(now);
        _bubbleActivityAggregator.recordPressureDelta(
            _pressurePa - _bubbleDetector.calibratedBaselinePa()
        );
    }

    if (bubbleDetected) _bubbleActivityAggregator.recordBubble();

    if (_config.rawPressureDiagnosticsEnabled)
    {
        Serial.print("PRESSURE,");
        Serial.print(now);
        Serial.print(',');
        Serial.println(_pressurePa, 2);
    }

    if (_config.eventDiagnosticsEnabled)
    {
        if (!wasCalibrated && _bubbleDetector.isCalibrated())
        {
            Serial.print("PRESSURE_CALIBRATED,baseline=");
            Serial.print(_bubbleDetector.baselinePa(), 2);
            Serial.print(",noise=");
            Serial.print(_bubbleDetector.noisePa(), 2);
            Serial.print(",trigger=");
            Serial.print(_bubbleDetector.triggerDeltaPa(), 2);
            Serial.print(",release=");
            Serial.println(_bubbleDetector.releaseDeltaPa(), 2);
        }

        if (bubbleDetected)
        {
            const BubbleEvent& event =
                _bubbleDetector.lastBubbleEvent();
            Serial.print("BUBBLE,");
            Serial.print(event.startedAtMs);
            Serial.print(',');
            Serial.print(event.durationMs);
            Serial.print(',');
            Serial.println(event.peakDeltaPa, 2);
        }

        if (
            _bubbleActivityAggregator.hasCompletedWindow() &&
            _bubbleActivityAggregator.completedWindowRevision() !=
                _diagnosedWindowRevision
        )
        {
            const BubbleActivityWindow& window =
                _bubbleActivityAggregator.completedWindow();
            Serial.print("BUBBLE_WINDOW,");
            Serial.print(window.startedAtMs);
            Serial.print(',');
            Serial.print(window.durationMs);
            Serial.print(',');
            Serial.print(window.bubbleCount);
            Serial.print(',');
            Serial.println(window.averagePressureDeltaPa, 2);
            _diagnosedWindowRevision =
                _bubbleActivityAggregator.completedWindowRevision();
        }
    }
}

void PressureSensor::onSessionRunning()
{
    if (_available) {
        const unsigned long now = millis();
        _bubbleDetector.onRunning(now);
        _bubbleActivityAggregator.resume(now);
    }
}

void PressureSensor::onSessionPaused()
{
    if (_available) {
        const unsigned long now = millis();
        _bubbleDetector.onPaused(now);
        _bubbleActivityAggregator.pause(now);
    }
}


bool PressureSensor::isAvailable() const
{
    return _available;
}


float PressureSensor::getPressurePa() const
{
    return _pressurePa;
}

bool PressureSensor::hasCompletedBubbleActivityWindow() const
{
    return _bubbleActivityAggregator.hasCompletedWindow();
}

const BubbleActivityWindow&
PressureSensor::completedBubbleActivityWindow() const
{
    return _bubbleActivityAggregator.completedWindow();
}

void PressureSensor::acknowledgeCompletedBubbleActivityWindow()
{
    _bubbleActivityAggregator.acknowledgeCompletedWindow();
}
