#include "config/SensorConfig.h"

#include <cmath>

SensorConfig SensorConfig::defaults()
{
    return {DEFAULT_EVENT_DIAGNOSTICS, DEFAULT_RAW_PRESSURE_DIAGNOSTICS,
            DEFAULT_SAMPLE_INTERVAL_MS, DEFAULT_CALIBRATION_MS,
            DEFAULT_MIN_TRIGGER_DELTA_PA, DEFAULT_NOISE_FACTOR,
            DEFAULT_RELEASE_FACTOR, DEFAULT_MIN_DURATION_MS,
            DEFAULT_MAX_DURATION_MS, DEFAULT_REFRACTORY_MS,
            DEFAULT_BASELINE_TRACKING_ALPHA, DEFAULT_ACTIVITY_WINDOW_MS,
            DEFAULT_TEMPERATURE_SEND_DELTA_C};
}

bool SensorConfig::validate(const char*& error) const
{
#define REQUIRE(condition, message) do { if (!(condition)) { error = message; return false; } } while (0)
    REQUIRE(sampleIntervalMs > 0 && sampleIntervalMs <= 60000, "sampleIntervalMs must be 1..60000");
    REQUIRE(calibrationMs > 0 && calibrationMs <= 3600000, "calibrationMs must be 1..3600000");
    REQUIRE(std::isfinite(minTriggerDeltaPa) && minTriggerDeltaPa > 0 && minTriggerDeltaPa <= 10000, "minTriggerDeltaPa must be finite and 0..10000");
    REQUIRE(std::isfinite(noiseFactor) && noiseFactor > 0 && noiseFactor <= 100, "noiseFactor must be finite and 0..100");
    REQUIRE(std::isfinite(releaseFactor) && releaseFactor > 0 && releaseFactor < 1, "releaseFactor must be finite and between 0 and 1");
    REQUIRE(minDurationMs > 0 && minDurationMs <= 60000, "minDurationMs must be 1..60000");
    REQUIRE(maxDurationMs >= minDurationMs && maxDurationMs <= 300000, "maxDurationMs must be >= minDurationMs and <= 300000");
    REQUIRE(refractoryMs <= 60000, "refractoryMs must be 0..60000");
    REQUIRE(std::isfinite(baselineTrackingAlpha) && baselineTrackingAlpha > 0 && baselineTrackingAlpha < 1, "baselineTrackingAlpha must be finite and between 0 and 1");
    REQUIRE(bubbleActivityWindowMs > 0 && bubbleActivityWindowMs <= 3600000, "windowMs must be 1..3600000");
    REQUIRE(std::isfinite(temperatureSendDeltaC) && temperatureSendDeltaC > 0 && temperatureSendDeltaC <= 50, "sendDeltaC must be finite and 0..50");
#undef REQUIRE
    error = nullptr;
    return true;
}

bool SensorConfig::operator==(const SensorConfig& other) const
{
    return eventDiagnosticsEnabled == other.eventDiagnosticsEnabled &&
        rawPressureDiagnosticsEnabled == other.rawPressureDiagnosticsEnabled &&
        sampleIntervalMs == other.sampleIntervalMs && calibrationMs == other.calibrationMs &&
        minTriggerDeltaPa == other.minTriggerDeltaPa && noiseFactor == other.noiseFactor &&
        releaseFactor == other.releaseFactor && minDurationMs == other.minDurationMs &&
        maxDurationMs == other.maxDurationMs && refractoryMs == other.refractoryMs &&
        baselineTrackingAlpha == other.baselineTrackingAlpha &&
        bubbleActivityWindowMs == other.bubbleActivityWindowMs &&
        temperatureSendDeltaC == other.temperatureSendDeltaC;
}
