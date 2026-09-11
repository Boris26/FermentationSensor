import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/storage/StorageMaintenance.cpp").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()


class StorageMaintenanceArchitectureTests(unittest.TestCase):
    def test_all_application_records_are_backed_up_restored_and_verified(self):
        for key in ("DEVICE_KEY", "WIFI_KEY", "TEMPERATURE_KEY", "GATEWAY_KEY", "SEQUENCE_KEY"):
            self.assertIn(f"backupRecord(_storage, {key}", SOURCE)
            self.assertIn(f"restoreRecord(_storage, {key}", SOURCE)
            self.assertIn(f"verifyRecord(_storage, {key}", SOURCE)

    def test_identity_and_sequence_are_mandatory(self):
        self.assertIn("backupRecord(_storage, DEVICE_KEY, true", SOURCE)
        self.assertIn("backupRecord(_storage, SEQUENCE_KEY, true", SOURCE)

    def test_reset_happens_only_after_complete_valid_backup(self):
        backup = SOURCE.index("STORAGE_MAINTENANCE_BACKUP_OK")
        reset = SOURCE.index("resetForMaintenance()")
        self.assertLess(backup, reset)
        self.assertIn('return fail("BACKUP")', SOURCE[:backup])

    def test_restore_and_verify_fail_closed(self):
        self.assertIn('return fail("RESTORE")', SOURCE)
        self.assertIn('return fail("VERIFY")', SOURCE)
        self.assertLess(SOURCE.index("STORAGE_MAINTENANCE_RESTORE_OK"),
                        SOURCE.index("STORAGE_MAINTENANCE_SUCCESS"))

    def test_sequence_record_is_validated_without_reinitialization(self):
        self.assertIn("MEASUREMENT_SEQUENCE_INITIAL_START", SOURCE)
        self.assertIn("nextBlockStart % MEASUREMENT_SEQUENCE_BLOCK_SIZE", SOURCE)
        self.assertNotRegex(SOURCE, r"nextBlockStart\s*=\s*MEASUREMENT_SEQUENCE_INITIAL_START")

    def test_internal_tdbs_record_is_not_copied(self):
        self.assertNotIn('"TDBS"', SOURCE)

    def test_runtime_reset_does_not_call_flash(self):
        body = MAIN.split("void resetRuntimeMeasurementState()", 1)[1].split("}", 1)[0]
        self.assertNotIn("flashStorage", body)
        self.assertIn("measurementOutbox.resetRuntimeState()", body)

    def test_factory_reset_and_compaction_are_separate(self):
        factory = MAIN.split("bool factoryReset()", 1)[1].split("}", 1)[0]
        maintenance = SOURCE.split("bool StorageMaintenance::compactPersistentStorage()", 1)[1]
        self.assertIn("flashStorage.factoryReset()", factory)
        self.assertNotIn("factoryReset", maintenance)

    def test_storage_command_is_only_processed_while_sequence_storage_failed(self):
        update = MAIN.split("void updateStorageMaintenanceCommand()", 1)[1].split(
            "enum class ErrorState", 1
        )[0]
        self.assertIn("if (measurementSequenceReady) return;", update)
        self.assertIn("while (Serial.available())", update)
        self.assertIn("strcmp(", update)
        self.assertIn("STORAGE_MAINTENANCE_COMMAND", update)

    def test_wrong_complete_command_is_ignored(self):
        update = MAIN.split("void updateStorageMaintenanceCommand()", 1)[1].split(
            "enum class ErrorState", 1
        )[0]
        self.assertIn("if (!commandMatches) continue;", update)
        self.assertIn("resetStorageMaintenanceCommandBuffer();", update)

    def test_compact_command_runs_maintenance_exactly_once(self):
        update = MAIN.split("void updateStorageMaintenanceCommand()", 1)[1].split(
            "enum class ErrorState", 1
        )[0]
        self.assertEqual(update.count("storageMaintenance.compactPersistentStorage()"), 1)

    def test_maintenance_is_announced_only_after_sequence_failure(self):
        setup = MAIN.split("void setup()", 1)[1].split("void loop()", 1)[0]
        failure = setup.split("if (!measurementSequenceReady)", 1)[1].split("}", 1)[0]
        self.assertIn("announceStorageMaintenanceAvailable()", failure)
        self.assertNotIn(
            "announceStorageMaintenanceAvailable()",
            setup.split("if (!measurementSequenceReady)", 1)[0],
        )

    def test_command_remains_available_in_normal_loop(self):
        loop = MAIN.split("void loop()", 1)[1]
        self.assertIn("updateStorageMaintenanceCommand();", loop)
        self.assertLess(
            loop.index("updateStorageMaintenanceCommand();"),
            loop.index("measurementButton.update();"),
        )

    def test_success_restarts_instead_of_reusing_allocator(self):
        update = MAIN.split("void updateStorageMaintenanceCommand()", 1)[1].split(
            "enum class ErrorState", 1
        )[0]
        self.assertIn("NVIC_SystemReset()", update)
        self.assertNotIn("measurementSequenceAllocator.begin()", update)

    def test_required_diagnostics_are_present_without_secret_values(self):
        for event in ("START", "BACKUP_OK", "RESET_OK", "RESTORE_OK", "SUCCESS"):
            self.assertIn(f"STORAGE_MAINTENANCE_{event}", SOURCE)
        self.assertIn("STORAGE_MAINTENANCE_FAILED,", SOURCE)
        self.assertNotIn("Serial.println(device", SOURCE)
        self.assertNotIn("Serial.println(wifi", SOURCE)


if __name__ == "__main__":
    unittest.main()
