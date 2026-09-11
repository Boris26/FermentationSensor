"""Host-side tests for the single-slot bubble activity transport state."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).parents[1]
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()


class BubbleActivityTransmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary_directory = tempfile.TemporaryDirectory()
        cls.executable = Path(cls.temporary_directory.name) / "outbox_test"
        harness = Path(cls.temporary_directory.name) / "outbox_test.cpp"
        harness.write_text(textwrap.dedent(r"""
            #include "network/BubbleActivityTransmission.h"
            #include "sensors/BubbleActivityAggregator.h"
            #include <cassert>
            #include <stdint.h>

            int main()
            {
                BubbleActivityAggregator aggregator(60000);
                aggregator.start(1000);
                aggregator.update(61000);
                assert(aggregator.hasCompletedWindow());

                BubbleActivityTransmission outbox(5000);
                assert(outbox.accept(aggregator.completedWindow()));
                assert(outbox.hasPending());
                assert(outbox.pending().sequence == 1);
                assert(outbox.pending().bubbleCount == 0);
                assert(outbox.pending().windowSeconds == 60);
                assert(!outbox.accept(aggregator.completedWindow()));

                // The outbox owns a copy, so releasing the source is safe.
                aggregator.acknowledgeCompletedWindow();
                assert(!aggregator.hasCompletedWindow());
                assert(outbox.pending().windowSeconds == 60);

                assert(!outbox.shouldSend(false, false, 100));
                assert(!outbox.shouldSend(true, false, 100));
                assert(outbox.shouldSend(true, true, 100));
                outbox.recordSuccessfulSend(100);
                assert(outbox.hasPending());
                assert(!outbox.shouldSend(true, true, 5099));
                assert(outbox.shouldSend(true, true, 5100));
                assert(outbox.isRetry());
                outbox.recordSuccessfulSend(5100);
                assert(!outbox.shouldSend(true, true, 5101));

                assert(!outbox.acknowledge(2));
                assert(outbox.hasPending());
                outbox.onTransportUnavailable();
                assert(outbox.hasPending());
                assert(!outbox.shouldSend(false, false, 5200));
                assert(outbox.shouldSend(true, true, 5200));
                assert(outbox.pending().sequence == 1);
                assert(outbox.acknowledge(1));
                assert(!outbox.hasPending());

                // Sequence rollover is the natural uint32_t rollover.
                BubbleActivityTransmission wrapping(5000, UINT32_MAX);
                BubbleActivityWindow window;
                assert(wrapping.accept(window));
                assert(wrapping.pending().sequence == UINT32_MAX);
                assert(wrapping.acknowledge(UINT32_MAX));
                assert(wrapping.accept(window));
                assert(wrapping.pending().sequence == 0);
                return 0;
            }
        """))
        subprocess.run([
            "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "include"), str(harness),
            str(ROOT / "src/network/BubbleActivityTransmission.cpp"),
            str(ROOT / "src/sensors/BubbleActivityAggregator.cpp"),
            "-o", str(cls.executable),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary_directory.cleanup()

    def test_single_slot_state_machine(self):
        subprocess.run([str(self.executable)], check=True)

    def test_wire_json_has_exact_fields(self):
        start = CLIENT.index("bool ServerClient::sendBubbleActivity(")
        end = CLIENT.index("bool ServerClient::takeBubbleActivityAcknowledgement", start)
        sender = CLIENT[start:end]
        for field in ("type", "deviceId", "sequence", "bubbleCount", "windowSeconds"):
            self.assertEqual(sender.count(f'\\"{field}\\"'), 1)
        for forbidden in ("pressurePa", "baseline", "noise", "startedAtMs"):
            self.assertNotIn(forbidden, sender)

    def test_main_releases_aggregator_only_after_copy(self):
        accept = MAIN.index("bubbleActivityTransmission.accept(")
        release = MAIN.index("acknowledgeCompletedBubbleActivityWindow();", accept)
        self.assertLess(accept, release)

    def test_register_ack_parser_remains_present(self):
        self.assertIn('if (type == "REGISTER_SENSOR_ACK")', CLIENT)
        self.assertIn('\\"BUBBLE_ACTIVITY_ACK\\"', CLIENT)

    def test_no_dynamic_queue_or_fermentation_logic(self):
        sources = "".join(
            (ROOT / path).read_text()
            for path in (
                "include/network/BubbleActivityTransmission.h",
                "src/network/BubbleActivityTransmission.cpp",
            )
        )
        for token in ("std::vector", "std::queue", "new ", "malloc", "beerId", "fermentation"):
            self.assertNotIn(token, sources)


if __name__ == "__main__":
    unittest.main()
