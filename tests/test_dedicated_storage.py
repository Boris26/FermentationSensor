from pathlib import Path
import unittest


ROOT = Path(__file__).parents[1]
FLASH = (ROOT / "src/storage/FlashStorage.cpp").read_text()


class DedicatedStorageTests(unittest.TestCase):
    def test_store_size_and_position_are_derived_from_legacy_bounds(self):
        self.assertIn("64U * 1024U", FLASH)
        self.assertIn("kv_get_default_flash_addresses", FLASH)
        self.assertIn("legacyStart - APPLICATION_STORAGE_SIZE_BYTES", FLASH)

    def test_store_is_separate_from_legacy_kv_region(self):
        self.assertIn("FlashIAPBlockDevice applicationBlockDevice", FLASH)
        self.assertIn("mbed::TDBStore applicationStore", FLASH)
        self.assertIn("LEGACY_KV_PREFIX", FLASH)

    def test_firmware_overlap_is_checked_before_store_init(self):
        overlap = FLASH.index("application store overlaps firmware")
        store = FLASH.index("FlashIAPBlockDevice applicationBlockDevice")
        self.assertLess(overlap, store)
        self.assertIn("__flash_binary_end", FLASH)

    def test_sequence_state_is_part_of_legacy_migration(self):
        self.assertIn('"measurement_sequence_v1"', FLASH)
        self.assertIn("Never\n        // overwrite an already copied key", FLASH)

    def test_legacy_cleanup_happens_after_layout_marker(self):
        migrate = FLASH.split("bool FlashStorage::migrateLegacyStore()", 1)[1]
        self.assertIn("writeLayoutMarker()", migrate)
        begin = FLASH.split("bool FlashStorage::begin()", 1)[1].split(
            "bool FlashStorage::setString", 1
        )[0]
        self.assertLess(begin.index("migrateLegacyStore()"), begin.index("cleanupLegacyStore()"))


if __name__ == "__main__":
    unittest.main()
