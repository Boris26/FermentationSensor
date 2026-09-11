from pathlib import Path
import unittest


ROOT = Path(__file__).parents[1]
FLASH = (ROOT / "src/storage/FlashStorage.cpp").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()


class KvDiagnosticsTests(unittest.TestCase):
    def test_boot_diagnostic_iterates_keys_without_reading_values(self):
        diagnostic = FLASH[FLASH.index("void FlashStorage::debugPrintEntries()") :]
        for call in (
            "kv_iterator_open(&iterator, KV_PREFIX)",
            "kv_iterator_next(iterator, key, sizeof(key))",
            "kv_get_info(key, &info)",
            "kv_iterator_close(iterator)",
        ):
            self.assertIn(call, diagnostic)
        self.assertNotIn("kv_get(", diagnostic)
        self.assertIn('Serial.print("KV_ENTRY,")', diagnostic)
        self.assertIn('Serial.print("KV_ENTRY_COUNT,")', diagnostic)
        self.assertIn('Serial.print("KV_LIVE_DATA_BYTES,")', diagnostic)
        self.assertNotIn("MBED_ERROR_ITEM_NOT_FOUND", diagnostic)

    def test_sequence_initialization_failure_is_explicit_and_gates_enqueues(self):
        self.assertIn(
            "measurementSequenceReady =\n        measurementSequenceAllocator.begin();",
            MAIN,
        )
        self.assertIn("MEASUREMENT_SEQUENCE_INITIALIZATION_FAILED", MAIN)
        self.assertGreaterEqual(MAIN.count("measurementSequenceReady &&"), 2)

    def test_media_full_has_symbolic_diagnostic(self):
        self.assertIn("MBED_ERROR_MEDIA_FULL", FLASH)
        self.assertIn("-2130771701", FLASH)


if __name__ == "__main__":
    unittest.main()
