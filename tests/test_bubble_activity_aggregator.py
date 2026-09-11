"""Host-side behavioural tests for fixed technical bubble-activity windows."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).parents[1]
CONFIG = (ROOT / "include/config/Config.h").read_text()
PRESSURE = (ROOT / "src/sensors/PressureSensor.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()


class BubbleActivityAggregatorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._temporary_directory = tempfile.TemporaryDirectory()
        cls.executable = Path(cls._temporary_directory.name) / "aggregator_test"
        harness = Path(cls._temporary_directory.name) / "aggregator_test.cpp"
        harness.write_text(textwrap.dedent(r"""
            #include "sensors/BubbleActivityAggregator.h"
            #include <cassert>
            #include <cmath>

            bool closeTo(float actual, float expected)
            {
                return std::fabs(actual - expected) < 0.0001f;
            }

            int main()
            {
                BubbleActivityAggregator windows(60000);
                assert(!windows.isActive());
                assert(!windows.hasCompletedWindow());
                windows.update(999999);
                windows.recordBubble();
                windows.recordPressureDelta(100.0f);
                assert(!windows.hasCompletedWindow());

                // Calibration completion is represented by explicitly starting
                // the first window; no state exists before that edge.
                windows.start(1000);
                assert(windows.isActive());
                assert(!windows.isPaused());
                windows.update(60999);
                windows.recordPressureDelta(1.0f);
                assert(!windows.hasCompletedWindow());
                windows.recordBubble();
                windows.update(61000);
                assert(windows.hasCompletedWindow());
                const BubbleActivityWindow& first = windows.completedWindow();
                assert(first.startedAtMs == 1000);
                assert(first.durationMs == 60000);
                assert(first.bubbleCount == 1);
                assert(first.pressureSampleCount == 1);
                assert(closeTo(first.averagePressureDeltaPa, 1.0f));
                assert(&first == &windows.completedWindow());
                assert(windows.hasCompletedWindow());
                windows.acknowledgeCompletedWindow();
                assert(!windows.hasCompletedWindow());

                // Completion immediately starts another window. An event
                // recognized on the boundary is assigned exactly once to it.
                windows.recordBubble();
                windows.recordPressureDelta(1.0f);
                windows.recordPressureDelta(2.0f);
                windows.recordPressureDelta(3.0f);
                windows.update(121000);
                BubbleActivityWindow second = windows.completedWindow();
                assert(second.startedAtMs == 61000);
                assert(second.bubbleCount == 1);
                assert(second.pressureSampleCount == 3);
                assert(closeTo(second.averagePressureDeltaPa, 2.0f));
                windows.acknowledgeCompletedWindow();

                // Multiple events are counted, and empty windows are retained.
                windows.recordBubble();
                windows.recordBubble();
                windows.recordBubble();
                windows.recordPressureDelta(-3.0f);
                windows.recordPressureDelta(1.0f);
                windows.update(181000);
                assert(windows.completedWindow().bubbleCount == 3);
                assert(closeTo(windows.completedWindow().averagePressureDeltaPa, -1.0f));
                windows.acknowledgeCompletedWindow();
                windows.recordPressureDelta(0.5f);
                windows.update(241000);
                assert(windows.hasCompletedWindow());
                assert(windows.completedWindow().bubbleCount == 0);
                assert(closeTo(windows.completedWindow().averagePressureDeltaPa, 0.5f));
                windows.acknowledgeCompletedWindow();

                // Only active RUNNING time advances the window.
                windows.update(271000);
                windows.recordBubble();
                windows.recordPressureDelta(2.0f);
                windows.pause(271000);
                windows.update(291000);
                windows.recordBubble();
                windows.recordPressureDelta(100.0f);
                assert(!windows.hasCompletedWindow());
                windows.resume(291000);
                windows.recordPressureDelta(4.0f);
                windows.update(320999);
                assert(!windows.hasCompletedWindow());
                windows.update(321000);
                BubbleActivityWindow paused = windows.completedWindow();
                assert(paused.bubbleCount == 1);
                assert(paused.pressureSampleCount == 2);
                assert(closeTo(paused.averagePressureDeltaPa, 3.0f));
                windows.acknowledgeCompletedWindow();

                // A fixed single slot is deterministic: latest completed wins.
                windows.update(441000);
                assert(windows.hasCompletedWindow());
                BubbleActivityWindow latest = windows.completedWindow();
                assert(latest.startedAtMs == 381000);
                assert(latest.bubbleCount == 0);
                assert(latest.pressureSampleCount == 0);
                windows.acknowledgeCompletedWindow();

                // Unsigned subtraction preserves elapsed-time behaviour over wrap.
                BubbleActivityAggregator wrapping(100);
                wrapping.start(~0UL - 49UL);
                wrapping.update(50UL);
                assert(wrapping.hasCompletedWindow());

                // Reset/new instance has no window before another calibration.
                windows.reset();
                assert(!windows.isActive());
                assert(!windows.hasCompletedWindow());
                BubbleActivityAggregator restarted(60000);
                assert(!restarted.isActive());
                return 0;
            }
        """))
        subprocess.run([
            "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "include"), str(harness),
            str(ROOT / "src/sensors/BubbleActivityAggregator.cpp"),
            "-o", str(cls.executable),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls._temporary_directory.cleanup()

    def test_window_scenarios(self):
        subprocess.run([str(self.executable)], check=True)

    def test_configuration_and_minimal_integration(self):
        self.assertIn("BUBBLE_ACTIVITY_WINDOW_MS = 60000", CONFIG)
        self.assertIn("_bubbleActivityAggregator.start(now);", PRESSURE)
        self.assertIn("_bubbleActivityAggregator.update(now);", PRESSURE)
        self.assertIn("_bubbleActivityAggregator.recordBubble();", PRESSURE)
        self.assertIn("_bubbleActivityAggregator.recordPressureDelta(", PRESSURE)
        self.assertIn("_bubbleDetector.calibratedBaselinePa()", PRESSURE)
        self.assertIn('Serial.print("BUBBLE_WINDOW,");', PRESSURE)
        diagnostics = PRESSURE[
            PRESSURE.index('Serial.print("BUBBLE_WINDOW,");'):
            PRESSURE.index("void PressureSensor::onSessionRunning()")
        ]
        self.assertNotIn("acknowledgeCompletedWindow", diagnostics)

    def test_gateway_does_not_send_individual_windows_from_aggregator(self):
        self.assertIn("BUBBLE_ACTIVITY", CLIENT)
        self.assertNotIn("BUBBLE_WINDOW", CLIENT)

    def test_aggregator_uses_no_dynamic_queue(self):
        header = (ROOT / "include/sensors/BubbleActivityAggregator.h").read_text()
        implementation = (
            ROOT / "src/sensors/BubbleActivityAggregator.cpp"
        ).read_text()
        for dynamic_storage in ("std::vector", "std::queue", "new ", "malloc"):
            self.assertNotIn(dynamic_storage, header + implementation)


if __name__ == "__main__":
    unittest.main()
