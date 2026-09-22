"""Regression checks for non-blocking temperature sensor readiness/recovery."""
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
CONFIG = (ROOT / "include/config/Config.h").read_text()
COORDINATOR = (ROOT / "src/app/SensorSessionCoordinator.cpp").read_text()
TEMPERATURE = (ROOT / "src/sensors/TemperatureSensor.cpp").read_text()


class TemperatureSensorRecoveryTests(unittest.TestCase):
    def test_expensive_rediscovery_is_recovery_only_and_rate_limited(self):
        self.assertIn("SENSOR_RECOVERY_SCAN_INTERVAL_MS = 5000", CONFIG)
        readiness = COORDINATOR.index("initializeSessionIfSensorsReady();")
        ready_return = COORDINATOR.index("if (_sensorsReady) return;", readiness)
        recovery_guard = COORDINATOR.index("SENSOR_RECOVERY_SCAN_INTERVAL_MS", ready_return)
        refresh = COORDINATOR.index("_temperatureSensor.refresh();", recovery_guard)
        self.assertLess(readiness, ready_return)
        self.assertLess(ready_return, recovery_guard)
        self.assertLess(recovery_guard, refresh)

    def test_successful_measurement_becomes_steady_state_health_signal(self):
        self.assertIn(
            "if (_measurementAttempted) {\n        return _lastMeasurementValid;\n    }",
            TEMPERATURE,
        )

    def test_boot_and_recovery_request_real_async_measurement(self):
        self.assertGreaterEqual(TEMPERATURE.count("_immediateMeasurementRequested = true;"), 3)
        refresh = TEMPERATURE.index("void TemperatureSensor::refresh()")
        self.assertIn("_measurementAttempted = false;", TEMPERATURE[refresh:])
        self.assertIn("_lastMeasurementValid = false;", TEMPERATURE[refresh:])
        self.assertIn("_immediateMeasurementRequested = true;", TEMPERATURE[refresh:])


if __name__ == "__main__":
    unittest.main()
