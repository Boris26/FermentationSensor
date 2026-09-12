from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
STARTER = (ROOT / "src/network/FermentationStarter.cpp").read_text()
COORDINATOR = (ROOT / "src/app/SensorSessionCoordinator.cpp").read_text()
APPLICATION = (ROOT / "src/app/FermentationSensorApplication.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()


class FermentationStartTests(unittest.TestCase):
    def test_assigned_finished_beer_id_is_used_in_endpoint(self):
        self.assertIn('parseStringField(message, "beerId", finishedBeerId)', CLIENT)
        self.assertIn('const String& beerId = _serverClient.finishedBeerId();', STARTER)
        self.assertIn('String("/finishedbeer/") + beerId +', STARTER)
        self.assertIn('"/start-fermentation"', STARTER)

    def test_post_has_no_body(self):
        self.assertIn("client.post(path);", STARTER)
        self.assertNotIn("fermentationStartedAt", STARTER)
        self.assertNotIn("Content-Type", STARTER)

    def test_only_initial_idle_to_running_transition_requests_start(self):
        transition = COORDINATOR.index("_lastState == MeasurementState::IDLE")
        request = COORDINATOR.index("_fermentationStarter.requestStart();", transition)
        self.assertLess(transition, request)
        self.assertIn("if (_requested) return;", STARTER)
        self.assertNotIn("requestStart", CLIENT)

    def test_http_failure_isolated_from_measurement_connection_and_ack(self):
        self.assertIn("_fermentationStarter.update();", APPLICATION)
        self.assertIn("_measurementTransport.update(_measurementSequenceReady);", APPLICATION)
        self.assertNotIn("disconnect()", STARTER)
        self.assertNotIn("requestReconnect", STARTER)
        self.assertIn("_nextAttemptMs = now + _retryIntervalMs;", STARTER)


if __name__ == "__main__":
    unittest.main()
