"""Static regression checks for deterministic WiFi AP selection and roaming."""
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
NETWORK = (ROOT / "src/network/NetworkManager.cpp").read_text()
HEADER = (ROOT / "include/network/NetworkManager.h").read_text()


class WifiRoamingTests(unittest.TestCase):
    def test_initial_connection_scans_before_connecting(self):
        begin = NETWORK[NETWORK.index("void NetworkManager::begin") :]
        self.assertIn("scheduleAccessPointSelection(millis(), \"initial\", false)", begin)
        self.assertIn("access point selection=pre-connect synchronous scan + targeted BSSID", NETWORK)
        self.assertNotIn("WiFi.begin(_credentials.ssid.c_str(), _credentials.password.c_str());\n}", begin[:begin.index("void NetworkManager::update")])

    def test_scan_is_synchronous_and_runs_after_a_short_disconnected_settle(self):
        self.assertIn("WIFI_AP_SCAN_SETTLE_MS = 500", NETWORK)
        self.assertIn("WiFi.scanNetworks(false, true)", NETWORK)
        self.assertNotIn("WiFi.scanComplete()", NETWORK)
        self.assertNotIn("WIFI_SCAN_RUNNING", NETWORK)
        self.assertNotIn("_apScanActive", HEADER)
        self.assertIn("WIFI_AP_SCAN_WAIT settleMs=", NETWORK)
        self.assertIn("mode=synchronous", NETWORK)
        self.assertIn("WIFI_AP_SCAN_RESULT result=", NETWORK)
        self.assertIn("durationMs=", NETWORK)

    def test_scan_starts_from_a_known_disconnected_state(self):
        selection = NETWORK[NETWORK.index("void NetworkManager::scheduleAccessPointSelection") :]
        self.assertIn("WiFi.disconnect(false, false);", selection)
        self.assertIn("_connectionStarted = false;", selection)

    def test_all_matching_ssid_candidates_are_logged(self):
        self.assertIn("WiFi.SSID(i) != _credentials.ssid", NETWORK)
        self.assertIn("WIFI_AP_CANDIDATE index=", NETWORK)
        self.assertIn("WIFI_AP_SCAN_TARGET_MATCHES count=", NETWORK)
        self.assertIn("WIFI_AP_SELECTED ssid=", NETWORK)
        self.assertIn("candidateRssi > bestRssi", NETWORK)

    def test_selected_ap_is_targeted_by_channel_and_bssid(self):
        self.assertIn("connectToAccessPoint(bestChannel, bestBssid, bestRssi)", NETWORK)
        self.assertIn("WIFI_CONNECT_SELECTED ssid=", NETWORK)
        self.assertIn("bssid,\n        true", NETWORK)

    def test_automatic_reconnect_is_disabled_so_it_cannot_race_selection(self):
        self.assertIn("WiFi.setAutoReconnect(false);", NETWORK)

    def test_weak_signal_triggers_disconnect_then_fresh_selection(self):
        self.assertIn("WIFI_ROAM_RSSI_THRESHOLD_DBM = -70", NETWORK)
        self.assertIn("WIFI_ROAM_CHECK_INTERVAL_MS = 60000", NETWORK)
        self.assertIn("scheduleAccessPointSelection(now, \"weak_signal\", true)", NETWORK)
        self.assertIn("WIFI_AP_SELECTION_DISCONNECT", NETWORK)
        self.assertIn("bool isRoaming() const { return _apSelectionActive; }", HEADER)

    def test_scan_failure_retries_then_uses_last_resort_automatic_connect(self):
        self.assertIn("WIFI_AP_SCAN_RETRY_DELAY_MS = 1500", NETWORK)
        self.assertIn("WIFI_AP_SCAN_MAX_FAILURES = 2", NETWORK)
        self.assertIn("WIFI_AP_SCAN_RETRY_SCHEDULED", NETWORK)
        self.assertIn("WIFI_AP_SCAN_GIVE_UP", NETWORK)
        self.assertIn("fallback=automatic", NETWORK)
        self.assertIn("connectAutomatically(\"scan_failure_fallback\")", NETWORK)


if __name__ == "__main__":
    unittest.main()
