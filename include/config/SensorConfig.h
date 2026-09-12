#pragma once

#include <stdint.h>

struct SensorConfig {
    static constexpr uint16_t VERSION = 1;
    static constexpr bool DEFAULT_EVENT_DIAGNOSTICS = true;
    static constexpr bool DEFAULT_RAW_PRESSURE_DIAGNOSTICS = false;
    static constexpr uint32_t DEFAULT_SAMPLE_INTERVAL_MS = 100;
    static constexpr uint32_t DEFAULT_CALIBRATION_MS = 300000;
    static constexpr float DEFAULT_MIN_TRIGGER_DELTA_PA = 0.50f;
    static constexpr float DEFAULT_NOISE_FACTOR = 5.0f;
    static constexpr float DEFAULT_RELEASE_FACTOR = 0.40f;
    static constexpr uint32_t DEFAULT_MIN_DURATION_MS = 100;
    static constexpr uint32_t DEFAULT_MAX_DURATION_MS = 3000;
    static constexpr uint32_t DEFAULT_REFRACTORY_MS = 500;
    static constexpr float DEFAULT_BASELINE_TRACKING_ALPHA = 0.001f;
    static constexpr uint32_t DEFAULT_ACTIVITY_WINDOW_MS = 60000;
    static constexpr float DEFAULT_TEMPERATURE_SEND_DELTA_C = 1.0f;
    bool eventDiagnosticsEnabled;
    bool rawPressureDiagnosticsEnabled;
    uint32_t sampleIntervalMs;
    uint32_t calibrationMs;
    float minTriggerDeltaPa;
    float noiseFactor;
    float releaseFactor;
    uint32_t minDurationMs;
    uint32_t maxDurationMs;
    uint32_t refractoryMs;
    float baselineTrackingAlpha;
    uint32_t bubbleActivityWindowMs;
    float temperatureSendDeltaC;

    static SensorConfig defaults();
    bool validate(const char*& error) const;
    bool operator==(const SensorConfig& other) const;
    bool operator!=(const SensorConfig& other) const { return !(*this == other); }
};
