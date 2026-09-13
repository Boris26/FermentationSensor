"""Architecture checks for server-driven FinishedBeer assignment."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()
CLIENT_HEADER = (ROOT / "include/network/ServerClient.h").read_text()
COORDINATOR = (ROOT / "src/app/SensorSessionCoordinator.cpp").read_text()


class MeasurementAssignmentTests(unittest.TestCase):
    def test_protocol_parses_assignment_and_echoes_exact_safe_id(self):
        self.assertIn('type == "ASSIGN_MEASUREMENT"', CLIENT)
        self.assertIn('parseStringField(message, "beerId", beerId)', CLIENT)
        self.assertIn('"ASSIGN_MEASUREMENT_ACK\\\",\\\"beerId\\\":\\\""', CLIENT)
        self.assertIn("sendAssignMeasurementAck(beerId)", COORDINATOR)

    def test_assignment_id_validation_is_retained(self):
        self.assertIn("isSafeFinishedBeerId", CLIENT_HEADER)
        self.assertIn("!isSafeFinishedBeerId(beerId)", CLIENT)
        self.assertIn("if (value.isEmpty()) return false;", CLIENT)
        self.assertIn("c == '-' || c == '_'", CLIENT)

    def test_idle_same_id_is_idempotent_but_changed_id_is_reset(self):
        assignment = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::handleMeasurementAssignments()"
        ):COORDINATOR.index(
            "void SensorSessionCoordinator::resetSessionRuntime(bool clearAssignment)"
        )]
        self.assertIn("_session.getState() == MeasurementState::IDLE", assignment)
        self.assertIn("_serverClient.finishedBeerId() == beerId", assignment)
        self.assertIn("if (!unchangedIdleAssignment)", assignment)
        self.assertIn("resetSessionRuntime(true);", assignment)
        self.assertIn("assignFinishedBeerContext(beerId)", assignment)

    def test_stop_and_reassignment_share_complete_runtime_reset(self):
        reset = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::resetSessionRuntime(bool clearAssignment)"
        ):]
        for operation in (
            "_session.stop();",
            "_transport.resetRuntimeState();",
            "_temperaturePolicy.resetRuntimeState();",
            "_pressureSensor.onSessionStopped();",
            "_serverClient.clearFinishedBeerContext();",
        ):
            self.assertIn(operation, reset)
        self.assertNotIn("Sequence", reset)
        self.assertNotIn("Flash", reset)

    def test_assignment_stays_on_registered_websocket_and_does_not_start(self):
        assignment = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::handleMeasurementAssignments()"
        ):COORDINATOR.index(
            "void SensorSessionCoordinator::resetSessionRuntime(bool clearAssignment)"
        )]
        self.assertNotIn("disconnect", assignment)
        self.assertNotIn("requestReconnect", assignment)
        self.assertNotIn("requestStart", assignment)
        self.assertNotIn("handleButtonPress", assignment)
        self.assertIn("session remains IDLE", assignment)

    def test_register_ack_still_uses_optional_shared_assignment_setter(self):
        registration = CLIENT[CLIENT.index(
            'if (type == "REGISTER_SENSOR_ACK")'
        ):CLIENT.index('if (type == "STOP_MEASUREMENT")')]
        self.assertIn('parseStringField(message, "beerId", finishedBeerId)', registration)
        self.assertIn("assignFinishedBeerContext(finishedBeerId)", registration)
        self.assertIn("_registered = true;", registration)


if __name__ == "__main__":
    unittest.main()
