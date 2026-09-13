"""Regression checks for the server-driven business session stop."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[1]
COORDINATOR = (ROOT / "src/app/SensorSessionCoordinator.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()
APPLICATION = (ROOT / "src/app/FermentationSensorApplication.cpp").read_text()


class MeasurementStopTests(unittest.TestCase):
    def test_session_stop_is_idempotent_and_button_transitions_are_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            (directory / "Arduino.h").write_text(textwrap.dedent("""
                #pragma once
                struct SerialStub { void println(const char*) {} };
                extern SerialStub Serial;
            """))
            harness = directory / "session.cpp"
            harness.write_text(textwrap.dedent("""
                #include "session/MeasurementSession.h"
                #include "Arduino.h"
                #include <cassert>
                SerialStub Serial;
                int main() {
                    MeasurementSession session;
                    session.begin();
                    session.stop();
                    assert(session.getState() == MeasurementState::IDLE);
                    session.handleButtonPress();
                    assert(session.getState() == MeasurementState::RUNNING);
                    session.stop();
                    assert(session.getState() == MeasurementState::IDLE);
                    session.handleButtonPress();
                    session.handleButtonPress();
                    assert(session.getState() == MeasurementState::PAUSED);
                    session.stop();
                    session.stop();
                    assert(session.getState() == MeasurementState::IDLE);
                    return 0;
                }
            """))
            executable = directory / "session"
            subprocess.run([
                "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                "-I", str(directory), "-I", str(ROOT / "include"), str(harness),
                str(ROOT / "src/session/MeasurementSession.cpp"), "-o", str(executable)
            ], check=True)
            subprocess.run([str(executable)], check=True)

    def test_stop_resets_every_session_runtime_component_before_ack(self):
        stop = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::handleStopMeasurementRequests()"
        ):]
        operations = [
            "_session.stop();",
            "_transport.resetRuntimeState();",
            "_temperaturePolicy.resetRuntimeState();",
            "_pressureSensor.onSessionStopped();",
            "_fermentationStarter.resetSession();",
            "_serverClient.clearFinishedBeerContext();",
            "_status.showMeasurementState(MeasurementState::IDLE);",
            "_serverClient.sendStopMeasurementAck();",
        ]
        positions = [stop.index(operation) for operation in operations]
        self.assertEqual(positions, sorted(positions))

    def test_protocol_ack_and_connection_are_preserved(self):
        self.assertIn('type == "STOP_MEASUREMENT"', CLIENT)
        self.assertIn('"{\\"type\\":\\"STOP_MEASUREMENT_ACK\\"}"', CLIENT)
        stop = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::handleStopMeasurementRequests()"
        ):]
        self.assertNotIn("disconnect()", stop)
        self.assertNotIn(".stop()", stop.replace("_session.stop()", ""))
        self.assertIn("_serverClient", APPLICATION)

    def test_sequence_allocator_and_persistent_state_are_not_reset(self):
        stop = COORDINATOR[COORDINATOR.index(
            "void SensorSessionCoordinator::handleStopMeasurementRequests()"
        ):]
        self.assertNotIn("Sequence", stop)
        self.assertNotIn("factoryReset", stop)
        self.assertNotIn("Flash", stop)


if __name__ == "__main__":
    unittest.main()
