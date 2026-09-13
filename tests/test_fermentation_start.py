from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
COORDINATOR = (ROOT / "src/app/SensorSessionCoordinator.cpp").read_text()
APPLICATION = (ROOT / "src/app/FermentationSensorApplication.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()
TRANSPORT = (ROOT / "src/app/MeasurementTransport.cpp").read_text()


class FermentationStartTests(unittest.TestCase):
    def test_idle_to_running_does_not_issue_direct_http_start(self):
        sources = "\n".join(
            path.read_text()
            for directory in (ROOT / "src", ROOT / "include")
            for path in directory.rglob("*")
            if path.is_file()
        )
        self.assertNotIn("start-fermentation", sources)
        self.assertNotIn("FermentationStarter", sources)
        self.assertIn("_session.handleButtonPress();", COORDINATOR)
        self.assertIn("current == MeasurementState::RUNNING", COORDINATOR)

    def test_measurement_transport_remains_active_after_session_input(self):
        session_input = APPLICATION.index("_sensorSession.updateSessionInput();")
        transport_update = APPLICATION.index(
            "_measurementTransport.update(_measurementSequenceReady);"
        )
        self.assertLess(session_input, transport_update)
        self.assertIn("_measurementTransport.update(_measurementSequenceReady);", APPLICATION)

    def test_existing_websocket_measurement_flow_is_unchanged(self):
        self.assertIn("_serverClient.sendMeasurement(pending", TRANSPORT)
        self.assertIn(
            "_serverClient.takeMeasurementAcknowledgement(acknowledgedSequence)",
            TRANSPORT,
        )
        self.assertIn('consume("\\\"MEASUREMENT_ACK\\\"")', CLIENT)


if __name__ == "__main__":
    unittest.main()
