"""Static regression checks for weak-signal WiFi roaming."""
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
NETWORK = (ROOT / "src/network/NetworkManager.cpp").read_text()
HEADER = (ROOT / "include/network/NetworkManager.h").read_text()


class WifiRoamingTests(unittest.TestCase):
    def test_roaming_only_starts_for_weak_signal_and_is_rate_limited(self):
        self.assertIn("WIFI_ROAM_RSSI_THRESHOLD_DBM = -70", NETWORK)
        self.assertIn("WIFI_ROAM_CHECK_INTERVAL_MS = 60000", NETWORK)
        self.assertIn("WIFI_ROAM_MIN_IMPROVEMENT_DB = 10", NETWORK)
        self.assertIn("currentRssi > WIFI_ROAM_RSSI_THRESHOLD_DBM", NETWORK)
        self.assertIn("now - _lastRoamCheckMs < WIFI_ROAM_CHECK_INTERVAL_MS", NETWORK)

    def test_roaming_scan_is_async(self):
        self.assertIn("WiFi.scanNetworks(true)", NETWORK)
        self.assertIn("WiFi.scanComplete()", NETWORK)
        self.assertIn("WIFI_SCAN_RUNNING", NETWORK)
        self.assertIn("_roamScanActive", HEADER)

    def test_roaming_uses_same_ssid_and_requires_a_better_bssid(self):
        self.assertIn("WiFi.SSID(i) != _credentials.ssid", NETWORK)
        self.assertIn("std::memcmp(candidateBssid, currentBssid", NETWORK)
        self.assertIn("improvement < WIFI_ROAM_MIN_IMPROVEMENT_DB", NETWORK)

    def test_roaming_handoff_targets_selected_channel_and_bssid(self):
        self.assertIn("WIFI_ROAM_SWITCH", NETWORK)
        self.assertIn("connectToAccessPoint(bestChannel, bestBssid, bestRssi)", NETWORK)
        self.assertIn("WiFi.disconnect(false, false)", NETWORK)
        self.assertIn("bssid,\n        true", NETWORK)


if __name__ == "__main__":
    unittest.main()
