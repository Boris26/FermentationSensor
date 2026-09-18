"""Static regression checks for the temporary field diagnostics."""
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
APP = (ROOT / "src/app/FermentationSensorApplication.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()
NETWORK = (ROOT / "src/network/NetworkManager.cpp").read_text()
TEMPERATURE = (ROOT / "src/sensors/TemperatureSensor.cpp").read_text()
PRESSURE_DRIVER = (ROOT / "src/sensors/Lwlp5000Driver.cpp").read_text()


class NetworkDisconnectDiagnosticTests(unittest.TestCase):
    def test_disconnect_separates_wifi_and_socket_evidence(self):
        for token in ("reason=", "wifiStatus=", "rssi=", "socketConnected=",
                      "lastRxAge=", "lastTxAge="):
            self.assertIn(token, CLIENT)

    def test_socket_lifecycle_and_wifi_edges_are_logged(self):
        for token in ("WS_CONNECT_ATTEMPT", "WS_CONNECTED", "WS_DISCONNECTED",
                      "WS_RECONNECT_SCHEDULED", "WS_TX", "WS_RX"):
            self.assertIn(token, CLIENT)
        self.assertIn("WIFI_STATUS_CHANGED", NETWORK)
        self.assertIn("WIFI_CONNECTED", NETWORK)

    def test_wifi_rssi_is_logged_periodically(self):
        self.assertIn("WIFI_RSSI_LOG_INTERVAL_MS = 30000", NETWORK)
        self.assertIn("WIFI_RSSI rssi=", NETWORK)

    def test_failed_wifi_connect_logs_native_status(self):
        for token in ("WIFI_CONNECT_FAILED", "WIFI_CONNECT_ATTEMPT",
                      "statusBefore=", "attemptAgeMs=",
                      "wifiStatusName"):
            self.assertIn(token, NETWORK)

    def test_connected_wifi_diagnostics_identify_access_point_without_scanning(self):
        for token in ("ssid=", "bssid=", "channel=", "WiFi.BSSIDstr()"):
            self.assertIn(token, NETWORK)
        self.assertNotIn("scanNetworks()", NETWORK)

    def test_slow_loop_log_has_requested_thresholds_and_components(self):
        for token in ("loopDuration > 100", "loopDuration > 500",
                      "loopDuration > 1000", "NetworkManager.update",
                      "TemperatureSensor.update", "PressureSensor.update"):
            self.assertIn(token, APP)

    def test_moderate_slow_loop_logging_is_rate_limited(self):
        self.assertIn("SLOW_LOOP_DETAIL_LOG_INTERVAL_MS = 60000", APP)
        self.assertIn("severeSlowLoop = loopDuration > 500", APP)
        self.assertIn("lastModerateSlowLoopLogMs == 0", APP)
        self.assertIn("severeSlowLoop || moderateSlowLoopDue", APP)

    def test_temperature_conversion_is_async_but_pressure_wait_is_known(self):
        self.assertIn("setWaitForConversion(false)", TEMPERATURE)
        self.assertIn("delay(CONVERSION_TIME_MS)", PRESSURE_DRIVER)


if __name__ == "__main__":
    unittest.main()
