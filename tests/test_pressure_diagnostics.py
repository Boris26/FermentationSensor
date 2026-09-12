"""Architecture checks for pressure sampling and serial diagnostics."""

from pathlib import Path
import unittest


ROOT = Path(__file__).parents[1]
CONFIG = (ROOT / "include/config/Config.h").read_text()
PRESSURE = (ROOT / "src/sensors/PressureSensor.cpp").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()


class PressureDiagnosticsTests(unittest.TestCase):
    def test_sampling_interval_is_central_configuration(self):
        self.assertIn("PRESSURE_SAMPLE_INTERVAL_MS = 100", CONFIG)
        self.assertIn("PRESSURE_SAMPLE_INTERVAL_MS", PRESSURE)
        self.assertNotIn("READ_INTERVAL_MS", PRESSURE)

    def test_raw_diagnostics_are_disabled_without_disabling_sampling(self):
        self.assertIn("PRESSURE_DIAGNOSTICS_ENABLED", CONFIG)
        self.assertIn("PRESSURE_RAW_DIAGNOSTICS_ENABLED = false", CONFIG)
        assignment = PRESSURE.index("_pressurePa =")
        raw_diagnostics = PRESSURE.index(
            "if (PRESSURE_RAW_DIAGNOSTICS_ENABLED)"
        )
        self.assertLess(assignment, raw_diagnostics)

    def test_pressure_update_remains_non_blocking(self):
        self.assertIn("now - lastReadMs <", PRESSURE)
        self.assertNotIn("delay(", PRESSURE)
        self.assertNotIn("while (", PRESSURE)

    def test_raw_line_is_isolated_behind_its_opt_in_flag(self):
        raw_start = PRESSURE.index("if (PRESSURE_RAW_DIAGNOSTICS_ENABLED)")
        event_start = PRESSURE.index("if (PRESSURE_DIAGNOSTICS_ENABLED)")
        raw_diagnostics = PRESSURE[raw_start:event_start]
        self.assertIn('Serial.print("PRESSURE,");', raw_diagnostics)
        self.assertIn("Serial.print(now);", raw_diagnostics)
        self.assertIn("Serial.println(_pressurePa, 2);", raw_diagnostics)
        self.assertNotIn("data.temperature", PRESSURE)

    def test_event_diagnostics_remain_enabled_independently(self):
        self.assertIn("PRESSURE_DIAGNOSTICS_ENABLED = true", CONFIG)
        event_start = PRESSURE.index("if (PRESSURE_DIAGNOSTICS_ENABLED)")
        events = PRESSURE[event_start:]
        for event in (
            'Serial.print("PRESSURE_CALIBRATED,baseline=");',
            'Serial.print("BUBBLE,");',
            'Serial.print("BUBBLE_WINDOW,");',
        ):
            self.assertIn(event, events)

    def test_pressure_sampling_remains_running_session_only(self):
        update = MAIN.index("pressureSensor.update();")
        guard = MAIN.rfind("measurementSession.isRunning()", 0, update)
        self.assertGreater(guard, MAIN.index("void loop()"))

    def test_raw_pressure_does_not_add_gateway_protocol(self):
        self.assertNotIn("PRESSURE,", CLIENT)
        self.assertEqual(CLIENT.count('\\"type\\":\\"TEMPERATURE_MEASUREMENT\\"'), 1)


if __name__ == "__main__":
    unittest.main()
