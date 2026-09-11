from pathlib import Path
import unittest


ROOT = Path(__file__).parents[1]
FLASH = (ROOT / "src/storage/FlashStorage.cpp").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()


class KvDiagnosticsTests(unittest.TestCase):
    def test_application_store_is_dedicated_and_64_kib(self):
        self.assertIn("APPLICATION_STORAGE_SIZE_BYTES =\n    64U * 1024U", FLASH)
        self.assertIn("kv_get_default_flash_addresses", FLASH)
        self.assertIn("legacyStart - APPLICATION_STORAGE_SIZE_BYTES", FLASH)
        self.assertIn("FlashIAPBlockDevice applicationBlockDevice", FLASH)
        self.assertIn("mbed::TDBStore applicationStore", FLASH)
        self.assertIn("__flash_binary_end", FLASH)
        self.assertIn("application store overlaps firmware", FLASH)

    def test_legacy_application_records_are_migrated_before_cleanup(self):
        for key in (
            "device_config",
            "wifi_config",
            "temperature_config",
            "gateway_cache",
            "measurement_sequence_v1",
        ):
            self.assertIn(f'"{key}"', FLASH)
        self.assertIn("FLASH_STORAGE_MIGRATION_SUCCESS", FLASH)
        self.assertIn("writeLayoutMarker()", FLASH)
        self.assertIn("FLASH_STORAGE_LEGACY_CLEANUP_OK", FLASH)
        self.assertLess(
            FLASH.index("writeLayoutMarker()"),
            FLASH.index("cleanupLegacyStore();"),
        )

    def test_boot_diagnostic_iterates_application_store_without_reading_values(self):
        diagnostic = FLASH[FLASH.index("void FlashStorage::debugPrintEntries()") :]
        for call in (
            "_store->iterator_open(&iterator)",
            "_store->iterator_next(",
            "_store->get_info(",
            "_store->iterator_close(iterator)",
        ):
            self.assertIn(call, diagnostic)
        self.assertNotIn("_store->get(\n", diagnostic)
        self.assertIn('Serial.print("KV_ENTRY,/app/")', diagnostic)
        self.assertIn('Serial.print("KV_ENTRY_COUNT,")', diagnostic)
        self.assertIn('Serial.print("KV_LIVE_DATA_BYTES,")', diagnostic)

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

    def test_factory_reset_clears_legacy_before_application_store(self):
        factory = FLASH.split("bool FlashStorage::factoryReset()", 1)[1].split(
            "bool FlashStorage::resetForMaintenance()", 1
        )[0]
        self.assertLess(factory.index("kv_reset(LEGACY_KV_PREFIX)"), factory.index("_store->reset()"))


if __name__ == "__main__":
    unittest.main()
