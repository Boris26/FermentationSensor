"""Host-side tests for the temperature transmission decision."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).parents[1]
MAIN = (ROOT / "src/main.cpp").read_text()


class TemperatureTransmissionPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._temporary_directory = tempfile.TemporaryDirectory()
        cls.executable = Path(cls._temporary_directory.name) / "policy_test"
        harness = Path(cls._temporary_directory.name) / "policy_test.cpp"
        harness.write_text(textwrap.dedent(
            r"""
            #include "network/TemperatureTransmissionPolicy.h"
            #include <cassert>

            int main()
            {
                TemperatureTransmissionPolicy policy(1.0f);

                // The first valid measurement is eligible.
                assert(policy.shouldSend(20.0f, 18.0f));
                policy.recordQueuedMeasurement(20.0f, 18.0f);

                // Changes accumulate against the last queued measurement.
                assert(!policy.shouldSend(20.5f, 18.4f));
                assert(policy.shouldSend(21.0f, 18.4f));
                policy.recordQueuedMeasurement(21.0f, 18.4f);

                // Either temperature can independently trigger a send.
                assert(policy.shouldSend(21.0f, 19.4f));
                policy.recordQueuedMeasurement(21.0f, 19.4f);

                // The threshold is inclusive.
                assert(policy.shouldSend(20.0f, 19.4f));
                assert(!policy.shouldSend(20.1f, 20.3f));

                // A failed enqueue does not update the reference, so the same
                // relevant change remains eligible on the next cycle.
                assert(policy.shouldSend(22.0f, 19.4f));
                assert(policy.shouldSend(22.0f, 19.4f));
                policy.recordQueuedMeasurement(22.0f, 19.4f);

                // A newly acknowledged transport session requests one snapshot.
                policy.requestCurrentMeasurement();
                assert(policy.isCurrentMeasurementRequested());
                assert(policy.shouldSend(22.1f, 19.5f));
                policy.recordQueuedMeasurement(22.1f, 19.5f);
                assert(!policy.isCurrentMeasurementRequested());
                assert(!policy.shouldSend(22.1f, 19.5f));

                return 0;
            }
            """
        ))
        subprocess.run(
            [
                "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "include"), str(harness),
                str(ROOT / "src/network/TemperatureTransmissionPolicy.cpp"),
                "-o", str(cls.executable),
            ],
            check=True,
        )

    @classmethod
    def tearDownClass(cls):
        cls._temporary_directory.cleanup()

    def test_policy_scenarios(self):
        subprocess.run([str(self.executable)], check=True)

    def test_only_successful_enqueue_updates_policy(self):
        send = MAIN.index("if (measurementOutbox.enqueueTemperature(")
        record = MAIN.index(
            "temperatureTransmissionPolicy.recordQueuedMeasurement(", send
        )
        self.assertLess(send, record)

    def test_registration_ack_edge_requests_current_measurement(self):
        self.assertIn("serverRegistered &&\n            !serverWasRegistered", MAIN)
        self.assertIn(".requestCurrentMeasurement();", MAIN)

    def test_pause_resume_does_not_force_transmission(self):
        button = MAIN.index("measurementSession.handleButtonPress();")
        button_flow = MAIN[button:]
        self.assertNotIn("requestCurrentMeasurement", button_flow)


if __name__ == "__main__":
    unittest.main()
