"""Host-side tests for durable, reserve-before-use measurement sequences."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).parents[1]


class MeasurementSequenceAllocatorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary_directory = tempfile.TemporaryDirectory()
        cls.executable = Path(cls.temporary_directory.name) / "sequence_test"
        harness = Path(cls.temporary_directory.name) / "sequence_test.cpp"
        harness.write_text(textwrap.dedent(r"""
            #include "network/MeasurementSequenceAllocator.h"
            #include <cassert>
            #include <stdint.h>
            #include <vector>

            class FakeStore : public MeasurementSequencePersistence
            {
            public:
                MeasurementSequenceLoadResult load(uint32_t& value) const override
                {
                    if (invalid) return MeasurementSequenceLoadResult::INVALID;
                    if (!present) return MeasurementSequenceLoadResult::NOT_FOUND;
                    value = persisted;
                    return MeasurementSequenceLoadResult::VALID;
                }
                bool save(uint32_t value) override
                {
                    saves.push_back(value);
                    if (failSave) return false;
                    present = true;
                    persisted = value;
                    return true;
                }
                bool present = false;
                bool invalid = false;
                bool failSave = false;
                uint32_t persisted = 0;
                std::vector<uint32_t> saves;
            };

            int main()
            {
                FakeStore store;
                MeasurementSequenceAllocator firstBoot(store);
                assert(firstBoot.begin());
                assert(store.saves.size() == 1);
                assert(store.saves[0] == MEASUREMENT_SEQUENCE_INITIAL_START +
                    MEASUREMENT_SEQUENCE_BLOCK_SIZE);

                uint32_t sequence = 0;
                assert(firstBoot.next(sequence));
                assert(sequence == MEASUREMENT_SEQUENCE_INITIAL_START);
                assert(firstBoot.next(sequence));
                assert(sequence == MEASUREMENT_SEQUENCE_INITIAL_START + 1);

                // The last value is valid; the following call reserves before use.
                for (uint32_t i = 2; i < MEASUREMENT_SEQUENCE_BLOCK_SIZE; ++i) {
                    assert(firstBoot.next(sequence));
                }
                assert(sequence == MEASUREMENT_SEQUENCE_INITIAL_START +
                    MEASUREMENT_SEQUENCE_BLOCK_SIZE - 1);
                assert(store.saves.size() == 1);
                assert(firstBoot.next(sequence));
                assert(sequence == MEASUREMENT_SEQUENCE_INITIAL_START +
                    MEASUREMENT_SEQUENCE_BLOCK_SIZE);
                assert(store.saves.size() == 2);

                // Reboots skip every previously reserved (possibly used) block.
                const uint32_t bootTwoStart = store.persisted;
                MeasurementSequenceAllocator secondBoot(store);
                assert(secondBoot.begin());
                assert(secondBoot.next(sequence));
                assert(sequence == bootTwoStart);
                const uint32_t bootThreeStart = store.persisted;
                MeasurementSequenceAllocator thirdBoot(store);
                assert(thirdBoot.begin());
                assert(thirdBoot.next(sequence));
                assert(sequence == bootThreeStart);
                assert(bootThreeStart > bootTwoStart);

                // A power loss immediately after save also skips that block.
                FakeStore powerLoss;
                MeasurementSequenceAllocator abandoned(powerLoss);
                assert(abandoned.begin());
                const uint32_t afterAbandonedBlock = powerLoss.persisted;
                MeasurementSequenceAllocator afterPowerLoss(powerLoss);
                assert(afterPowerLoss.begin());
                assert(afterPowerLoss.next(sequence));
                assert(sequence == afterAbandonedBlock);

                FakeStore writeFailure;
                writeFailure.failSave = true;
                MeasurementSequenceAllocator failed(writeFailure);
                assert(!failed.begin());
                sequence = 123;
                assert(!failed.next(sequence));
                assert(sequence == 123);

                // A later block save failure exposes no value from that block.
                FakeStore rolloverFailure;
                MeasurementSequenceAllocator rollover(rolloverFailure);
                assert(rollover.begin());
                rolloverFailure.failSave = true;
                for (uint32_t i = 0; i < MEASUREMENT_SEQUENCE_BLOCK_SIZE; ++i) {
                    assert(rollover.next(sequence));
                }
                sequence = 456;
                assert(!rollover.next(sequence));
                assert(sequence == 456);

                FakeStore corrupt;
                corrupt.invalid = true;
                MeasurementSequenceAllocator invalid(corrupt);
                assert(!invalid.begin());
                assert(!invalid.next(sequence));

                // The terminal marker cannot be reserved as a usable block: its
                // successor cannot be represented, so allocation never wraps.
                FakeStore exhausted;
                exhausted.present = true;
                exhausted.persisted = 0xFFFF0000U;
                MeasurementSequenceAllocator terminal(exhausted);
                assert(!terminal.begin());
                assert(!terminal.next(sequence));
                assert(exhausted.saves.empty());
                return 0;
            }
        """))
        subprocess.run([
            "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "include"), str(harness),
            str(ROOT / "src/network/MeasurementSequenceAllocator.cpp"),
            "-o", str(cls.executable),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary_directory.cleanup()

    def test_reservation_reboot_failures_and_exhaustion(self):
        subprocess.run([str(self.executable)], check=True)

    def test_store_is_versioned_and_validates_persisted_state(self):
        header = (ROOT / "include/storage/MeasurementSequenceStore.h").read_text()
        source = (ROOT / "src/storage/MeasurementSequenceStore.cpp").read_text()
        self.assertIn('STORAGE_KEY = "measurement_sequence_v1"', header)
        self.assertIn("STORAGE_MAGIC", header)
        self.assertIn("STORAGE_VERSION", header)
        self.assertIn("stored.magic != STORAGE_MAGIC", source)
        self.assertIn("stored.version != STORAGE_VERSION", source)
        self.assertIn("!isValidNextBlockStart(stored.nextBlockStart)", source)


if __name__ == "__main__":
    unittest.main()
