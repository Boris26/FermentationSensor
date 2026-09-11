"""Host-side tests for the fixed generic measurement ring buffer."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).parents[1]
HEADER = (ROOT / "include/network/MeasurementOutbox.h").read_text()
SOURCE = (ROOT / "src/network/MeasurementOutbox.cpp").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()


class MeasurementOutboxTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary_directory = tempfile.TemporaryDirectory()
        cls.executable = Path(cls.temporary_directory.name) / "outbox_test"
        harness = Path(cls.temporary_directory.name) / "outbox_test.cpp"
        harness.write_text(textwrap.dedent(r"""
            #include "network/MeasurementOutbox.h"
            #include <cassert>
            #include <stdint.h>

            int main()
            {
                MeasurementOutbox outbox(5000, 100);
                assert(outbox.empty());
                assert(outbox.capacity() == MEASUREMENT_OUTBOX_CAPACITY);

                assert(outbox.enqueueTemperature(20.0f, 18.0f, true, 0.08f, 1000));
                BubbleActivityWindow zeroWindow;
                zeroWindow.durationMs = 60000;
                zeroWindow.completedAtMs = 61000;
                zeroWindow.bubbleCount = 0;
                zeroWindow.averagePressureDeltaPa = -1.25f;
                zeroWindow.pressureSampleCount = 600;
                assert(outbox.enqueueBubbleActivity(zeroWindow));

                assert(outbox.size() == 2);
                assert(outbox.front().type == MeasurementType::TEMPERATURE);
                assert(outbox.front().sequence == 100);
                assert(outbox.front().payload.temperature.beerTemperature == 20.0f);
                assert(outbox.front().payload.temperature.ambientTemperature == 18.0f);
                assert(outbox.front().payload.temperature.pressureAvailable);
                assert(outbox.front().payload.temperature.pressurePa == 0.08f);
                assert(outbox.front().ageSeconds(121000) == 120);
                assert(outbox.back().sequence == 101);

                // Local send success retains the immutable front until ACK.
                assert(!outbox.shouldSend(false, false, 100));
                assert(!outbox.shouldSend(true, false, 100));
                assert(outbox.shouldSend(true, true, 100));
                outbox.recordSuccessfulSend(100);
                assert(outbox.front().sequence == 100);
                assert(!outbox.shouldSend(true, true, 5099));
                assert(outbox.shouldSend(true, true, 5100));
                assert(outbox.front().ageSeconds(181000) == 180);
                assert(!outbox.acknowledge(999));
                assert(outbox.acknowledge(100));

                // Mixed types use one sequence namespace and preserve FIFO.
                assert(outbox.front().type == MeasurementType::BUBBLE_ACTIVITY);
                assert(outbox.front().sequence == 101);
                assert(outbox.front().payload.bubbleActivity.bubbleCount == 0);
                assert(outbox.front().payload.bubbleActivity.windowSeconds == 60);
                assert(outbox.front().payload.bubbleActivity.averagePressureDeltaPa == -1.25f);
                assert(outbox.front().ageSeconds(186000) == 125);
                assert(outbox.shouldSend(true, true, 5101));
                outbox.recordSuccessfulSend(5101);
                assert(outbox.front().payload.bubbleActivity.averagePressureDeltaPa == -1.25f);
                outbox.onTransportUnavailable();
                assert(outbox.front().sequence == 101);
                assert(outbox.shouldSend(true, true, 5102));
                assert(outbox.acknowledge(101));

                // Fill the configured ring and verify DROP OLDEST overflow.
                for (size_t i = 0; i < outbox.capacity(); ++i) {
                    assert(outbox.enqueueTemperature(i, i, false, 0, i));
                }
                assert(outbox.full());
                const uint32_t droppedSequence = outbox.front().sequence;
                assert(outbox.enqueueBubbleActivity(zeroWindow));
                assert(outbox.full());
                assert(outbox.droppedCount() == 1);
                assert(outbox.lastDroppedType() == MeasurementType::TEMPERATURE);
                assert(outbox.lastDroppedSequence() == droppedSequence);
                assert(!outbox.acknowledge(droppedSequence));
                assert(outbox.front().sequence == droppedSequence + 1);
                assert(outbox.back().type == MeasurementType::BUBBLE_ACTIVITY);
                assert(outbox.back().payload.bubbleActivity.averagePressureDeltaPa == -1.25f);

                BubbleActivityWindow missingPressure;
                missingPressure.durationMs = 60000;
                assert(!outbox.enqueueBubbleActivity(missingPressure));

                // Unsigned subtraction naturally handles millis() rollover.
                OutboxEntry wrapped = {};
                wrapped.capturedAtMs = UINT32_MAX - 999;
                assert(wrapped.ageSeconds(1000) == 2);

                assert(sizeof(OutboxEntry) == 28);
                return 0;
            }
        """))
        subprocess.run([
            "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "include"), str(harness),
            str(ROOT / "src/network/MeasurementOutbox.cpp"), "-o", str(cls.executable),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary_directory.cleanup()

    def test_fifo_ack_retry_age_and_overflow(self):
        subprocess.run([str(self.executable)], check=True)

    def test_no_dynamic_queue_or_non_measurement_payload(self):
        sources = HEADER + SOURCE
        for token in ("std::vector", "std::deque", "std::queue", "new ", "malloc"):
            self.assertNotIn(token, sources)
        for token in ("BubbleEvent", "baseline", "noise", "trigger", "REGISTER_SENSOR"):
            self.assertNotIn(token, sources)

    def test_main_transfers_ownership_only_after_enqueue(self):
        enqueue = MAIN.index("measurementOutbox.enqueueBubbleActivity(")
        acknowledge = MAIN.index("acknowledgeCompletedBubbleActivityWindow();", enqueue)
        self.assertLess(enqueue, acknowledge)
        self.assertIn("measurementOutbox.enqueueTemperature(", MAIN)
        self.assertIn("measurementSession.isRunning()", MAIN)
        self.assertIn("updateMeasurementOutbox();", MAIN)
        transport = MAIN[MAIN.index("void updateMeasurementOutbox()"):]
        self.assertNotIn("measurementSession.isRunning()", transport.split("void setup()", 1)[0])
        for diagnostic in (
            "MEASUREMENT_BUFFERED,", "MEASUREMENT_SENT,", "MEASUREMENT_RETRY,",
            "MEASUREMENT_ACK,", "MEASUREMENT_OUTBOX_OVERFLOW,",
        ):
            self.assertIn(diagnostic, MAIN)

    def test_wire_protocol_and_dynamic_age(self):
        sender = CLIENT[CLIENT.index("bool ServerClient::sendMeasurement("):]
        for field in (
            "TEMPERATURE_MEASUREMENT", "BUBBLE_ACTIVITY", "sequence",
            "measurementAgeSeconds", "windowEndAgeSeconds",
            "averagePressureDeltaPa",
        ):
            self.assertIn(field, sender)
        self.assertIn("measurement.ageSeconds(nowMs)", sender)
        self.assertNotIn("BUBBLE_ACTIVITY_ACK", CLIENT)
        self.assertIn('consume("\\\"MEASUREMENT_ACK\\\"")', CLIENT)

    def test_largest_expected_json_fits_websocket_buffer(self):
        device_id = "x" * 36
        temperature = (
            '{"type":"TEMPERATURE_MEASUREMENT","deviceId":"' + device_id +
            '","sequence":4294967295,"beerTemperature":-127.0,'
            '"ambientTemperature":-127.0,"pressurePa":-500.00,'
            '"measurementAgeSeconds":4294967}'
        )
        bubble = (
            '{"type":"BUBBLE_ACTIVITY","deviceId":"' + device_id +
            '","sequence":4294967295,"bubbleCount":65535,'
            '"windowSeconds":4294967,"averagePressureDeltaPa":-500.00,'
            '"windowEndAgeSeconds":4294967}'
        )
        self.assertLessEqual(len(temperature), 256)
        self.assertLessEqual(len(bubble), 256)


if __name__ == "__main__":
    unittest.main()
