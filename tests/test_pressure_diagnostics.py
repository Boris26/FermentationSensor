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

    def test_diagnostics_can_be_disabled_without_disabling_sampling(self):
        self.assertIn("PRESSURE_DIAGNOSTICS_ENABLED", CONFIG)
        assignment = PRESSURE.index("_pressurePa =")
        diagnostics = PRESSURE.index("if (PRESSURE_DIAGNOSTICS_ENABLED)")
        self.assertLess(assignment, diagnostics)

    def test_pressure_update_remains_non_blocking(self):
        self.assertIn("now - lastReadMs <", PRESSURE)
        self.assertNotIn("delay(", PRESSURE)
        self.assertNotIn("while (", PRESSURE)

    def test_machine_readable_line_has_only_required_fields(self):
        self.assertIn('Serial.print("PRESSURE,");', PRESSURE)
        self.assertIn("Serial.print(now);", PRESSURE)
        self.assertIn("Serial.println(_pressurePa, 2);", PRESSURE)
        self.assertNotIn("data.temperature", PRESSURE)

    def test_pressure_sampling_remains_running_session_only(self):
        update = MAIN.index("pressureSensor.update();")
        guard = MAIN.rfind("measurementSession.isRunning()", 0, update)
        self.assertGreater(guard, MAIN.index("void loop()"))

    def test_raw_pressure_does_not_add_gateway_protocol(self):
        self.assertNotIn("PRESSURE,", CLIENT)
        self.assertEqual(CLIENT.count('\\"type\\":\\"TEMPERATURE_MEASUREMENT\\"'), 1)


if __name__ == "__main__":
    unittest.main()
