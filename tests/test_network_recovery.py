"""Architectural checks for autonomous layered network recovery."""
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
SERVER = (ROOT / "src/network/ServerClient.cpp").read_text()
SERVER_HEADER = (ROOT / "include/network/ServerClient.h").read_text()
NETWORK = (ROOT / "src/network/NetworkManager.cpp").read_text()
NETWORK_HEADER = (ROOT / "include/network/NetworkManager.h").read_text()
GATEWAY = (ROOT / "src/app/GatewayConnectionManager.cpp").read_text()
APP = (ROOT / "src/app/FermentationSensorApplication.cpp").read_text()


class NetworkRecoveryTests(unittest.TestCase):
    def test_reconnect_backoff_is_bounded(self):
        self.assertIn("{1000, 2000, 5000, 10000, 30000}", SERVER)
        self.assertNotIn("delay(", SERVER)

    def test_socket_and_tcp_clients_are_destroyed_periodically(self):
        self.assertIn("SOCKET_RECREATE_FAILURE_INTERVAL", SERVER)
        destroy = SERVER[SERVER.index("void ServerClient::destroySocketClient()") :]
        self.assertIn("_webSocketClient->stop();", destroy)
        self.assertIn("delete _webSocketClient;", destroy)
        self.assertIn("_webSocketClient = nullptr;", destroy)
        self.assertIn("_wifiClient.stop();", destroy)

    def test_repeated_tcp_failure_escalates_to_wifi(self):
        self.assertIn("WIFI_RECOVERY_FAILURE_THRESHOLD", GATEWAY)
        self.assertIn("_networkManager.requestReconnect(", GATEWAY)
        self.assertIn("WiFi.disconnect(true, false);", NETWORK)
        self.assertIn("WiFi.mode(WIFI_STA);", NETWORK)

    def test_wifi_diagnostics_cover_route_and_signal(self):
        for call in ("WiFi.status()", "WiFi.localIP()", "WiFi.gatewayIP()",
                     "WiFi.dnsIP()", "WiFi.RSSI()"):
            self.assertIn(call, NETWORK)

    def test_application_loop_remains_cooperative(self):
        network = APP.index("_networkManager.update();")
        sensors = APP.index("_sensorSession.updateMeasurements", network)
        self.assertLess(network, sensors)
        self.assertNotIn("delay(", APP[APP.index("void FermentationSensorApplication::update()") :])

    def test_gateway_waits_for_initial_wifi_roaming(self):
        self.assertIn("bool isRoaming() const", NETWORK_HEADER)
        self.assertIn("if (_networkManager.isRoaming())", GATEWAY)
        roaming_check = GATEWAY.index("if (_networkManager.isRoaming())")
        cached_connect = GATEWAY.index("Gateway: trying last-known endpoint cache first.")
        self.assertLess(roaming_check, cached_connect)

    def test_gateway_enforces_socket_backoff_after_blocking_connect_attempt(self):
        self.assertIn("currentReconnectDelayMs() const", SERVER_HEADER)
        self.assertIn("_nextServerUpdateMs", GATEWAY)
        self.assertIn("socketRetryGuardActive", GATEWAY)
        self.assertIn("Gateway: socket retry guard delay=", GATEWAY)

    def test_cached_endpoint_does_not_get_second_attempt_in_same_loop(self):
        cached = GATEWAY.index("Gateway: trying last-known endpoint cache first.")
        discovery = GATEWAY.index("Gateway: endpoint failed; starting rediscovery.", cached)
        first_return = GATEWAY.index("return;", discovery)
        endpoint_update = GATEWAY.index("if (_endpointActive)", cached)
        self.assertLess(first_return, endpoint_update)


if __name__ == "__main__":
    unittest.main()
