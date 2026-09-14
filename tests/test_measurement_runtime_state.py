"""Regression checks for publishing the MeasurementSession runtime state."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()
CLIENT_HEADER = (ROOT / "include/network/ServerClient.h").read_text()
COORDINATOR = (ROOT / "src/app/SensorSessionCoordinator.cpp").read_text()


class MeasurementRuntimeStateTests(unittest.TestCase):
    def test_protocol_contains_device_and_each_supported_state_but_no_beer(self):
        send = CLIENT[CLIENT.index(
            "bool ServerClient::sendMeasurementState"
        ):CLIENT.index(
            "bool ServerClient::takeMeasurementAcknowledgement"
        )]
        self.assertIn('"MEASUREMENT_STATE_CHANGED', send)
        self.assertIn("_deviceIdentity.getDeviceId()", send)
        self.assertNotIn("beerId", send)
        for state in ("IDLE", "RUNNING", "PAUSED"):
            self.assertIn(f'MeasurementState::{state}', send)
            self.assertIn(f'stateName = "{state}"', send)
        self.assertIn(
            'sendTextMessage(message, "measurement runtime state")', send
        )

    def test_button_edges_publish_once_without_changing_immediate_measurement(self):
        update = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::updateSessionInput()"
        ):COORDINATOR.index(
            "void SensorSessionCoordinator::handleRegistrationEstablished()"
        )]
        self.assertEqual(update.count("publishMeasurementState(current);"), 1)
        self.assertIn("if (current != _lastState)", update)
        self.assertIn("_temperatureSensor.requestImmediateMeasurement();", update)
        self.assertIn("_initialMeasurementPending = true;", update)
        self.assertIn("_pressureSensor.onSessionPaused();", update)

    def test_unchanged_state_is_suppressed(self):
        publish = COORDINATOR[COORDINATOR.index(
            "bool SensorSessionCoordinator::publishMeasurementState("
        ):COORDINATOR.index(
            "void SensorSessionCoordinator::handleStopMeasurementRequests()"
        )]
        self.assertIn(
            "!force && _hasPublishedState && state == _lastPublishedState",
            publish,
        )
        self.assertIn("_lastPublishedState = state;", publish)
        self.assertLess(
            publish.index("_serverClient.sendMeasurementState(state)"),
            publish.index("_lastPublishedState = state;"),
        )

    def test_stop_and_assignment_reset_publish_idle_only_on_a_real_transition(self):
        reset = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::resetSessionRuntime"
        ):]
        self.assertIn("const MeasurementState previousState", reset)
        self.assertIn("_session.stop();", reset)
        self.assertIn("previousState != MeasurementState::IDLE", reset)
        self.assertIn("publishMeasurementState(MeasurementState::IDLE);", reset)

        stop = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::handleStopMeasurementRequests()"
        ):COORDINATOR.index(
            "void SensorSessionCoordinator::handleMeasurementAssignments()"
        )]
        assignment = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::handleMeasurementAssignments()"
        ):COORDINATOR.index(
            "void SensorSessionCoordinator::resetSessionRuntime"
        )]
        self.assertIn("resetSessionRuntime(true);", stop)
        self.assertIn("resetSessionRuntime(true);", assignment)
        self.assertLess(
            stop.index("resetSessionRuntime(true);"),
            stop.index("sendStopMeasurementAck()"),
        )

    def test_each_new_registration_is_a_one_shot_forced_resynchronization(self):
        registration = CLIENT[CLIENT.index(
            'if (type == "REGISTER_SENSOR_ACK")'
        ):CLIENT.index(
            'if (type == "STOP_MEASUREMENT")'
        )]
        self.assertIn("_registrationEstablished = true;", registration)
        self.assertIn("bool takeRegistrationEstablished();", CLIENT_HEADER)
        self.assertIn("_registrationEstablished = false;", CLIENT)

        resync = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::handleRegistrationEstablished()"
        ):COORDINATOR.index(
            "bool SensorSessionCoordinator::publishMeasurementState("
        )]
        self.assertIn("_session.getState()", resync)
        self.assertIn("publishMeasurementState(current, true)", resync)
        for state in ("IDLE", "RUNNING", "PAUSED"):
            self.assertIn(f"MeasurementState::{state}", resync)

    def test_reconnect_transport_does_not_mutate_measurement_session(self):
        reconnect = CLIENT[CLIENT.index(
            "void ServerClient::disconnect()"
        ):CLIENT.index(
            "void ServerClient::sendRegistration()"
        )]
        self.assertNotIn("MeasurementSession", reconnect)
        self.assertNotIn("MeasurementState::", reconnect)


if __name__ == "__main__":
    unittest.main()
