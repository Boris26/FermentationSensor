#include "sensors/PressureBubbleDetector.h"

#include <algorithm>
#include <cmath>

PressureBubbleDetector::PressureBubbleDetector(const PressureBubbleConfig& config)
    : _config(config)
{
}

void PressureBubbleDetector::onRunning(unsigned long nowMs)
{
    if (_paused && _mode == Mode::CALIBRATING) {
        _calibrationStartedAtMs += nowMs - _pausedAtMs;
    }
    _paused = false;
    _detectionState = DetectionState::IDLE;
    if (_mode == Mode::UNCALIBRATED) beginCalibration(nowMs);
}

void PressureBubbleDetector::onPaused(unsigned long nowMs)
{
    _paused = true;
    _pausedAtMs = nowMs;
    _detectionState = DetectionState::IDLE;
}

bool PressureBubbleDetector::processSample(unsigned long nowMs, float pressurePa)
{
    if (_paused) return false;

    if (_mode == Mode::CALIBRATING) {
        if (nowMs - _calibrationStartedAtMs < _config.calibrationMs) {
            addCalibrationSample(pressurePa);
            return false;
        }
        finishCalibration();
        return false;
    }

    if (_mode != Mode::MONITORING) return false;

    if (_detectionState == DetectionState::REFRACTORY) {
        if (nowMs - _refractoryStartedAtMs < _config.refractoryMs) return false;
        _detectionState = DetectionState::IDLE;
    }

    const float deltaPa = pressurePa - _baselinePa;
    if (_detectionState == DetectionState::IDLE) {
        if (deltaPa >= _triggerDeltaPa) {
            _detectionState = DetectionState::ACTIVE;
            _bubbleStartedAtMs = nowMs;
            _peakDeltaPa = deltaPa;
        } else {
            _baselinePa += _config.baselineTrackingAlpha * (pressurePa - _baselinePa);
        }
        return false;
    }

    if (deltaPa > _peakDeltaPa) _peakDeltaPa = deltaPa;
    const unsigned long durationMs = nowMs - _bubbleStartedAtMs;
    if (durationMs > _config.maximumDurationMs) {
        enterRefractory(nowMs);
        return false;
    }
    if (deltaPa > _releaseDeltaPa) return false;

    if (durationMs >= _config.minimumDurationMs) {
        _lastBubbleEvent.startedAtMs = _bubbleStartedAtMs;
        _lastBubbleEvent.durationMs = durationMs;
        _lastBubbleEvent.peakDeltaPa = _peakDeltaPa;
        ++_totalBubbleCount;
        enterRefractory(nowMs);
        return true;
    }

    enterRefractory(nowMs);
    return false;
}

void PressureBubbleDetector::beginCalibration(unsigned long nowMs)
{
    clearCalibrationData();
    _calibrationStartedAtMs = nowMs;
    _mode = Mode::CALIBRATING;
    _totalBubbleCount = 0;
    _lastBubbleEvent = BubbleEvent();
}

void PressureBubbleDetector::addCalibrationSample(float pressurePa)
{
    _blockSum += pressurePa;
    _blockSumSquares += pressurePa * pressurePa;
    ++_blockSampleCount;
    if (_blockSampleCount < CALIBRATION_BLOCK_SAMPLES) return;
    if (_calibrationBlockCount < MAX_CALIBRATION_BLOCKS) {
        const float count = static_cast<float>(_blockSampleCount);
        const float mean = _blockSum / count;
        _calibrationBlocks[_calibrationBlockCount] = mean;
        _calibrationNoiseBlocks[_calibrationBlockCount] =
            std::sqrt(std::max(0.0f, (_blockSumSquares / count) - (mean * mean)));
        ++_calibrationBlockCount;
    }
    _blockSum = 0.0f;
    _blockSumSquares = 0.0f;
    _blockSampleCount = 0;
}

void PressureBubbleDetector::finishCalibration()
{
    if (_blockSampleCount > 0 && _calibrationBlockCount < MAX_CALIBRATION_BLOCKS) {
        const float count = static_cast<float>(_blockSampleCount);
        const float mean = _blockSum / count;
        _calibrationBlocks[_calibrationBlockCount] = mean;
        _calibrationNoiseBlocks[_calibrationBlockCount] =
            std::sqrt(std::max(0.0f, (_blockSumSquares / count) - (mean * mean)));
        ++_calibrationBlockCount;
    }

    if (_calibrationBlockCount == 0) {
        _mode = Mode::UNCALIBRATED;
        clearCalibrationData();
        return;
    }

    _baselinePa = median(_calibrationBlocks, _calibrationBlockCount);
    _calibratedBaselinePa = _baselinePa;
    _noisePa = median(_calibrationNoiseBlocks, _calibrationBlockCount);
    _triggerDeltaPa = std::max(
        _config.minimumTriggerDeltaPa,
        _noisePa * _config.noiseFactor
    );
    _releaseDeltaPa = _triggerDeltaPa * _config.releaseFactor;
    _mode = Mode::MONITORING;
    _detectionState = DetectionState::IDLE;
    clearCalibrationData();
}

void PressureBubbleDetector::clearCalibrationData()
{
    std::fill(_calibrationBlocks, _calibrationBlocks + MAX_CALIBRATION_BLOCKS, 0.0f);
    std::fill(
        _calibrationNoiseBlocks,
        _calibrationNoiseBlocks + MAX_CALIBRATION_BLOCKS,
        0.0f
    );
    _calibrationBlockCount = 0;
    _blockSum = 0.0f;
    _blockSumSquares = 0.0f;
    _blockSampleCount = 0;
}

void PressureBubbleDetector::enterRefractory(unsigned long nowMs)
{
    _detectionState = DetectionState::REFRACTORY;
    _refractoryStartedAtMs = nowMs;
}

float PressureBubbleDetector::median(float* values, size_t count)
{
    std::sort(values, values + count);
    const size_t middle = count / 2;
    return count % 2 == 0
        ? (values[middle - 1] + values[middle]) * 0.5f
        : values[middle];
}

bool PressureBubbleDetector::isCalibrating() const { return _mode == Mode::CALIBRATING; }
bool PressureBubbleDetector::isCalibrated() const { return _mode == Mode::MONITORING; }
bool PressureBubbleDetector::isBubbleActive() const { return _detectionState == DetectionState::ACTIVE; }
float PressureBubbleDetector::baselinePa() const { return _baselinePa; }
float PressureBubbleDetector::calibratedBaselinePa() const
{
    return _calibratedBaselinePa;
}
float PressureBubbleDetector::noisePa() const { return _noisePa; }
float PressureBubbleDetector::triggerDeltaPa() const { return _triggerDeltaPa; }
float PressureBubbleDetector::releaseDeltaPa() const { return _releaseDeltaPa; }
uint32_t PressureBubbleDetector::totalBubbleCount() const { return _totalBubbleCount; }
const BubbleEvent& PressureBubbleDetector::lastBubbleEvent() const { return _lastBubbleEvent; }
size_t PressureBubbleDetector::calibrationBlockCount() const { return _calibrationBlockCount; }
