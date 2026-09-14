"""Regression checks for the fresh measurement requested on session start/resume."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
COORDINATOR = (ROOT / "src/app/SensorSessionCoordinator.cpp").read_text()
TEMPERATURE = (ROOT / "src/sensors/TemperatureSensor.cpp").read_text()
TEMPERATURE_HEADER = (ROOT / "include/sensors/TemperatureSensor.h").read_text()


class InitialSessionMeasurementTests(unittest.TestCase):
    def test_running_transition_requests_a_fresh_measurement(self):
        running = COORDINATOR.index("current == MeasurementState::RUNNING")
        paused = COORDINATOR.index("current == MeasurementState::PAUSED", running)
        branch = COORDINATOR[running:paused]
        self.assertIn("_temperatureSensor.requestImmediateMeasurement();", branch)
        self.assertIn("_initialMeasurementPending = true;", branch)

    def test_request_bypasses_interval_but_keeps_conversion_asynchronous(self):
        self.assertIn("void requestImmediateMeasurement();", TEMPERATURE_HEADER)
        self.assertIn(
            "!_immediateMeasurementRequested &&\n"
            "            now - _lastMeasurementMs <",
            TEMPERATURE,
        )
        request = TEMPERATURE[TEMPERATURE.index(
            "void TemperatureSensor::requestImmediateMeasurement()"
        ):]
        self.assertIn("_conversionInProgress = false;", request)
        self.assertIn("_immediateMeasurementRequested = true;", request)
        self.assertNotIn("delay(", request.split(
            "void TemperatureSensor::cancelImmediateMeasurementRequest()", 1
        )[0])
        self.assertLess(
            TEMPERATURE.index("_sensors.requestTemperatures();"),
            TEMPERATURE.index("_sensors.getTempC(address);")
        )

    def test_initial_value_uses_transport_once_and_bypasses_policy_once(self):
        queue = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::queueTemperatureIfEligible"
        ):COORDINATOR.index(
            "void SensorSessionCoordinator::updateMeasurements"
        )]
        self.assertIn(
            "if (!forceInitialMeasurement && !_temperaturePolicy.shouldSend(",
            queue,
        )
        self.assertIn("_transport.bufferTemperature(", queue)
        self.assertIn("_pressureSensor.isAvailable()", queue)
        self.assertIn("_temperaturePolicy.recordQueuedMeasurement", queue)
        self.assertIn("_initialMeasurementPending = false;", queue)
        self.assertLess(
            queue.index("_transport.bufferTemperature("),
            queue.index("_initialMeasurementPending = false;")
        )

    def test_stop_cancels_unconsumed_request(self):
        reset = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::resetSessionRuntime"
        ):]
        self.assertIn("_temperatureSensor.cancelImmediateMeasurementRequest();", reset)
        self.assertIn("_initialMeasurementPending = false;", reset)


if __name__ == "__main__":
    unittest.main()
