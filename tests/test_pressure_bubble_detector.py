"""Host-side behavioural tests for pressure calibration and bubble detection."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).parents[1]
CONFIG = (ROOT / "include/config/Config.h").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()
PRESSURE = (ROOT / "src/sensors/PressureSensor.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()


class PressureBubbleDetectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._temporary_directory = tempfile.TemporaryDirectory()
        cls.executable = Path(cls._temporary_directory.name) / "pressure_test"
        harness = Path(cls._temporary_directory.name) / "pressure_test.cpp"
        harness.write_text(textwrap.dedent(r"""
            #include "sensors/PressureBubbleDetector.h"
            #include <cassert>
            #include <cmath>

            PressureBubbleConfig config(float noiseFactor = 5.0f)
            {
                return {5000, 0.5f, noiseFactor, 0.4f, 100, 3000, 500, 0.001f};
            }

            void calibrate(PressureBubbleDetector& detector, float noise = 0.05f,
                           bool withOutlier = false, unsigned long duration = 5000)
            {
                detector.onRunning(0);
                for (unsigned long now = 0; now < duration; now += 100) {
                    float value = 10.0f + ((now / 100) % 2 ? noise : -noise);
                    if (withOutlier && now == 1200) value = 1000.0f;
                    assert(!detector.processSample(now, value));
                    assert(detector.totalBubbleCount() == 0);
                }
                assert(detector.isCalibrating());
                assert(!detector.processSample(duration, 10.0f));
                assert(detector.isCalibrated());
                assert(detector.calibrationBlockCount() == 0);
            }

            int main()
            {
                PressureBubbleDetector quiet(config());
                calibrate(quiet);
                assert(std::fabs(quiet.baselinePa() - 10.0f) < 0.02f);
                assert(quiet.noisePa() > 0.04f);
                assert(std::fabs(quiet.triggerDeltaPa() - 0.5f) < 0.001f);

                PressureBubbleDetector noisy(config());
                calibrate(noisy, 0.3f);
                assert(noisy.triggerDeltaPa() > quiet.triggerDeltaPa());

                PressureBubbleConfig outlierConfig = config();
                outlierConfig.calibrationMs = 15000;
                PressureBubbleDetector outlier(outlierConfig);
                calibrate(outlier, 0.05f, true, 15000);
                assert(std::fabs(outlier.baselinePa() - 10.0f) < 0.1f);

                // Below trigger does not create an event; idle drift tracks slowly.
                const float baseline = quiet.baselinePa();
                const float calibratedBaseline = quiet.calibratedBaselinePa();
                assert(!quiet.processSample(5100, baseline + 0.2f));
                assert(quiet.totalBubbleCount() == 0);
                assert(quiet.baselinePa() > baseline);
                assert(quiet.calibratedBaselinePa() == calibratedBaseline);

                // One released event records start, duration, and peak.
                assert(!quiet.processSample(5200, quiet.baselinePa() + 0.6f));
                const float activeBaseline = quiet.baselinePa();
                assert(!quiet.processSample(5300, activeBaseline + 1.2f));
                assert(quiet.baselinePa() == activeBaseline);
                assert(quiet.processSample(5500, activeBaseline));
                assert(quiet.totalBubbleCount() == 1);
                assert(quiet.lastBubbleEvent().startedAtMs == 5200);
                assert(quiet.lastBubbleEvent().durationMs == 300);
                assert(std::fabs(quiet.lastBubbleEvent().peakDeltaPa - 1.2f) < 0.001f);

                // Refractory ringing is ignored, then a later peak is accepted.
                assert(!quiet.processSample(5600, activeBaseline + 2.0f));
                assert(!quiet.processSample(6000, activeBaseline + 0.7f));
                assert(!quiet.processSample(6100, activeBaseline + 0.7f));
                assert(quiet.processSample(6250, activeBaseline));
                assert(quiet.totalBubbleCount() == 2);

                // Too-short events are discarded and enter refractory.
                PressureBubbleDetector shortEvent(config());
                calibrate(shortEvent);
                assert(!shortEvent.processSample(5100, 11.0f));
                assert(!shortEvent.processSample(5150, 10.0f));
                assert(shortEvent.totalBubbleCount() == 0);

                // Overlong events cannot leave the detector stuck active.
                PressureBubbleDetector overlong(config());
                calibrate(overlong);
                assert(!overlong.processSample(5100, 11.0f));
                assert(!overlong.processSample(8201, 11.0f));
                assert(!overlong.isBubbleActive());
                assert(overlong.totalBubbleCount() == 0);

                // Pause drops an active event; resume retains calibration.
                PressureBubbleDetector paused(config());
                calibrate(paused);
                assert(!paused.processSample(5100, 11.0f));
                paused.onPaused(5200);
                assert(!paused.isBubbleActive());
                paused.onRunning(10000);
                assert(paused.isCalibrated());
                assert(!paused.processSample(10100, 10.0f));
                assert(paused.totalBubbleCount() == 0);

                // Paused time does not complete an in-progress calibration.
                PressureBubbleDetector calibrationPause(config());
                calibrationPause.onRunning(0);
                for (unsigned long now = 0; now < 1000; now += 100)
                    calibrationPause.processSample(now, 10.0f);
                calibrationPause.onPaused(1000);
                calibrationPause.onRunning(10000);
                calibrationPause.processSample(13000, 10.0f);
                assert(calibrationPause.isCalibrating());
                calibrationPause.processSample(14000, 10.0f);
                assert(calibrationPause.isCalibrated());

                // A new instance always starts without persisted calibration.
                PressureBubbleDetector restarted(config());
                assert(!restarted.isCalibrated());
                restarted.onRunning(0);
                assert(restarted.isCalibrating());
                return 0;
            }
        """))
        subprocess.run([
            "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "include"), str(harness),
            str(ROOT / "src/sensors/PressureBubbleDetector.cpp"),
            "-o", str(cls.executable),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls._temporary_directory.cleanup()

    def test_calibration_and_detection_scenarios(self):
        subprocess.run([str(self.executable)], check=True)

    def test_production_configuration_start_values(self):
        for setting in (
            "PRESSURE_CALIBRATION_MS = 300000",
            "BUBBLE_MIN_TRIGGER_DELTA_PA = 0.50f",
            "BUBBLE_NOISE_FACTOR = 5.0f",
            "BUBBLE_RELEASE_FACTOR = 0.40f",
            "BUBBLE_MIN_DURATION_MS = 100",
            "BUBBLE_MAX_DURATION_MS = 3000",
            "BUBBLE_REFRACTORY_MS = 500",
            "PRESSURE_BASELINE_TRACKING_ALPHA = 0.001f",
        ):
            self.assertIn(setting, CONFIG)

    def test_session_edges_control_pressure_processing(self):
        button = MAIN.index("measurementSession.handleButtonPress();")
        flow = MAIN[button:]
        self.assertIn("pressureSensor.onSessionRunning();", flow)
        self.assertIn("pressureSensor.onSessionPaused();", flow)

    def test_diagnostics_and_gateway_protocol_are_separate(self):
        self.assertIn('Serial.print("PRESSURE_CALIBRATED,baseline=");', PRESSURE)
        self.assertIn('Serial.print("BUBBLE,");', PRESSURE)
        self.assertNotIn("BUBBLE,", CLIENT)
        self.assertNotIn("PRESSURE_CALIBRATED", CLIENT)


if __name__ == "__main__":
    unittest.main()
