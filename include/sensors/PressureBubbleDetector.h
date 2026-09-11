#pragma once

#include <stddef.h>
#include <stdint.h>

struct BubbleEvent
{
    unsigned long startedAtMs = 0;
    unsigned long durationMs = 0;
    float peakDeltaPa = 0.0f;
};

struct PressureBubbleConfig
{
    unsigned long calibrationMs;
    float minimumTriggerDeltaPa;
    float noiseFactor;
    float releaseFactor;
    unsigned long minimumDurationMs;
    unsigned long maximumDurationMs;
    unsigned long refractoryMs;
    float baselineTrackingAlpha;
};

// Host-testable pressure calibration and event state machine. Calibration stores
// only one mean per five-second block (at 10 Hz), never the raw sample history.
class PressureBubbleDetector
{
public:
    explicit PressureBubbleDetector(const PressureBubbleConfig& config);

    void onRunning(unsigned long nowMs);
    void onPaused(unsigned long nowMs);
    bool processSample(unsigned long nowMs, float pressurePa);

    bool isCalibrating() const;
    bool isCalibrated() const;
    bool isBubbleActive() const;

    float baselinePa() const;
    float noisePa() const;
    float triggerDeltaPa() const;
    float releaseDeltaPa() const;
    uint32_t totalBubbleCount() const;
    const BubbleEvent& lastBubbleEvent() const;
    size_t calibrationBlockCount() const;

private:
    enum class Mode { UNCALIBRATED, CALIBRATING, MONITORING };
    enum class DetectionState { IDLE, ACTIVE, REFRACTORY };

    static constexpr size_t CALIBRATION_BLOCK_SAMPLES = 50;
    static constexpr size_t MAX_CALIBRATION_BLOCKS = 64;

    void beginCalibration(unsigned long nowMs);
    void addCalibrationSample(float pressurePa);
    void finishCalibration();
    void clearCalibrationData();
    void enterRefractory(unsigned long nowMs);
    static float median(float* values, size_t count);

    PressureBubbleConfig _config;
    Mode _mode = Mode::UNCALIBRATED;
    DetectionState _detectionState = DetectionState::IDLE;
    bool _paused = true;
    unsigned long _calibrationStartedAtMs = 0;
    float _calibrationBlocks[MAX_CALIBRATION_BLOCKS] = {};
    float _calibrationNoiseBlocks[MAX_CALIBRATION_BLOCKS] = {};
    size_t _calibrationBlockCount = 0;
    float _blockSum = 0.0f;
    float _blockSumSquares = 0.0f;
    size_t _blockSampleCount = 0;
    unsigned long _pausedAtMs = 0;

    float _baselinePa = 0.0f;
    float _noisePa = 0.0f;
    float _triggerDeltaPa = 0.0f;
    float _releaseDeltaPa = 0.0f;
    unsigned long _bubbleStartedAtMs = 0;
    unsigned long _refractoryStartedAtMs = 0;
    float _peakDeltaPa = 0.0f;
    uint32_t _totalBubbleCount = 0;
    BubbleEvent _lastBubbleEvent;
};
