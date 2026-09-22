"""Regression checks for restarting after WiFi setup credentials are stored."""
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
HEADER = (ROOT / "include/network/WifiSetupPortal.h").read_text()
SOURCE = (ROOT / "src/network/WifiSetupPortal.cpp").read_text()


class WifiSetupRestartTests(unittest.TestCase):
    def test_restart_is_delayed_after_successful_store(self):
        self.assertIn("RESTART_DELAY_MS = 2000", HEADER)
        self.assertIn("_restartPending = true;", SOURCE)
        self.assertIn("_restartScheduledMs = millis();", SOURCE)
        self.assertIn("ESP.restart();", SOURCE)

        store = SOURCE.index("_credentialStore.save(credentials)")
        success = SOURCE.index("sendSuccessPage(client);", store)
        scheduled = SOURCE.index("_restartScheduledMs = millis();", success)
        restart = SOURCE.index("ESP.restart();")

        self.assertLess(store, success)
        self.assertLess(success, scheduled)
        self.assertLess(restart, store)

    def test_pending_restart_stops_accepting_more_setup_requests(self):
        pending = SOURCE.index("if (_restartPending) {")
        available = SOURCE.index("_server.available()")
        self.assertLess(pending, available)

    def test_success_page_tells_user_about_restart(self):
        self.assertIn(
            "Der Sensor startet neu und verbindet sich anschließend mit dem WLAN.",
            SOURCE,
        )


if __name__ == "__main__":
    unittest.main()
