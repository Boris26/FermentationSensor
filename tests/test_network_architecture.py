"""Host-side architectural regression checks for the embedded network flow.

These checks deliberately avoid mocking Arduino drivers. They protect the policy and
protocol invariants which can be verified without target hardware; packet and
socket behaviour is covered by the manual hardware test plan in the README/report.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
MAIN = (ROOT / "src/main.cpp").read_text()
CONNECTION_MANAGER = (ROOT / "src/app/GatewayConnectionManager.cpp").read_text()
DISCOVERY = (ROOT / "src/network/GatewayDiscovery.cpp").read_text()
CLIENT = (ROOT / "src/network/ServerClient.cpp").read_text()
ENDPOINT = (ROOT / "include/network/GatewayEndpoint.h").read_text()
STORE = (ROOT / "src/storage/GatewayEndpointStore.cpp").read_text()
CONFIG = (ROOT / "include/config/Config.h").read_text()


class NetworkArchitectureTests(unittest.TestCase):
    def test_valid_cache_is_loaded_and_tried_first(self):
        load = MAIN.index("gatewayEndpointStore.load()")
        begin = MAIN.index("serverClient.begin(cached)")
        discovery = MAIN.index("gatewayDiscovery.start()", begin)
        self.assertLess(load, begin)
        self.assertLess(begin, discovery)
        self.assertIn("if (cached.isValid())", MAIN[load:begin])

    def test_invalid_cache_enters_discovery(self):
        self.assertIn("} else {\n            gatewayDiscovery.start();", MAIN)

    def test_successful_cache_does_not_discover(self):
        self.assertIn("if (serverClient.isRegistered()) cachedEndpointPending = false;", MAIN)

    def test_failed_initial_cache_enters_discovery(self):
        self.assertIn("cachedEndpointPending\n            ? 1", MAIN)

    def test_endpoint_validation(self):
        for check in ('port == 0', '!path.startsWith("/")',
                      'protocolVersion != 1', 'parsed.fromString(address.c_str())',
                      'parsed != IPAddress()'):
            self.assertIn(check, ENDPOINT)

    def test_unspecified_ipv4_is_not_a_valid_endpoint(self):
        parse = ENDPOINT.index("parsed.fromString(address.c_str())")
        reject_unspecified = ENDPOINT.index("parsed != IPAddress()", parse)
        self.assertGreater(reject_unspecified, parse)

    def test_valid_lan_ipv4_is_not_restricted(self):
        start = ENDPOINT.index("bool isValid() const")
        validation = ENDPOINT[
            start:ENDPOINT.index("\n    }", start)
        ]
        self.assertIn("parsed.fromString(address.c_str())", validation)
        self.assertNotIn("192.168.", validation)

    def test_dns_sd_records_and_service_type(self):
        for token in ('_brewferment._tcp.local', 'DNS_PTR', 'DNS_SRV',
                      'DNS_TXT', 'DNS_A', 'path=', 'version='):
            self.assertIn(token, DISCOVERY)

    def test_missing_path_wrong_version_and_invalid_port_cannot_complete(self):
        self.assertIn("candidate.path = _path", DISCOVERY)
        self.assertIn("candidate.protocolVersion = _version", DISCOVERY)
        self.assertIn("candidate.port = _port", DISCOVERY)
        self.assertIn("if (!candidate.isValid()) return", DISCOVERY)

    def test_ptr_srv_txt_without_a_record_cannot_complete(self):
        completion = DISCOVERY[DISCOVERY.index("void GatewayDiscovery::tryComplete()") :]
        address_guard = completion.index("if (_address == IPAddress()) return;")
        result = completion.index("_hasResult = true;")
        self.assertLess(address_guard, result)

    def test_discovery_keeps_querying_until_a_record_arrives(self):
        self.assertIn("else if (_address == IPAddress()) sendQuery(_host, DNS_A);", DISCOVERY)
        self.assertIn(
            "_address = IPAddress(data[offset], data[offset + 1], "
            "data[offset + 2], data[offset + 3]);",
            DISCOVERY,
        )
        completion = DISCOVERY[DISCOVERY.index("void GatewayDiscovery::tryComplete()") :]
        self.assertLess(
            completion.index("if (_address == IPAddress()) return;"),
            completion.index("candidate.address = _address.toString();"),
        )

    def test_no_service_times_out_non_blocking(self):
        self.assertIn("DISCOVERY_TIMEOUT_MS", DISCOVERY)
        self.assertNotIn("delay(", DISCOVERY)
        self.assertIn("GATEWAY_DISCOVERY_RETRY_INTERVAL_MS", MAIN)

    def test_discovered_endpoint_replaces_cache_before_connect(self):
        save = MAIN.index("gatewayEndpointStore.save(discovered)")
        connect = MAIN.index("serverClient.begin(discovered)")
        self.assertLess(save, connect)

    def test_unspecified_legacy_cache_is_ignored_and_replaced(self):
        self.assertIn("return endpoint.isValid() ? endpoint : GatewayEndpoint{};", STORE)
        self.assertIn("if (cached.isValid())", CONNECTION_MANAGER)
        self.assertIn("} else {\n            startDiscovery();", CONNECTION_MANAGER)
        self.assertLess(
            CONNECTION_MANAGER.index("_endpointStore.save(discovered)"),
            CONNECTION_MANAGER.index("_serverClient.begin(discovered)"),
        )

    def test_runtime_endpoint_retried_before_rediscovery(self):
        update = MAIN.index("serverClient.update()")
        threshold = MAIN.index("GATEWAY_REDISCOVERY_FAILURE_THRESHOLD", update)
        rediscover = MAIN.index("gatewayDiscovery.start()", threshold)
        self.assertLess(update, threshold)
        self.assertLess(threshold, rediscover)
        self.assertIn("= 3", CONFIG)

    def test_no_socket_or_discovery_work_without_wifi(self):
        disconnected = MAIN.index("if (!wifiConnected)")
        connected_edge = MAIN.index("if (!wifiWasConnected)", disconnected)
        branch = MAIN[disconnected:connected_edge]
        self.assertIn("gatewayDiscovery.stop()", branch)
        self.assertIn("return;", branch)

    def test_every_websocket_connect_registers_fresh_session(self):
        success = CLIENT.index("_connected = true;")
        registration = CLIENT.index("sendRegistration();", success)
        self.assertIn("_registered = false;", CLIENT[success:registration])
        self.assertLess(success, registration)
        self.assertIn('\\"REGISTER_SENSOR\\"', CLIENT)
        self.assertIn('"REGISTER_SENSOR_ACK"', CLIENT)

    def test_cache_contains_all_runtime_endpoint_fields(self):
        for field in ("address", "port", "path", "protocolVersion"):
            self.assertIn("stored." + field, STORE)

    def test_unchanged_cache_skips_second_persistence_write(self):
        load = STORE.index("const GatewayEndpoint existing = load();")
        unchanged = STORE.index("GatewayEndpointStore: cache unchanged.", load)
        write = STORE.index("_storage.setBytes", unchanged)
        comparison = STORE[load:unchanged]
        for field in ("address", "port", "path", "protocolVersion"):
            self.assertIn("existing." + field + " == endpoint." + field, comparison)
        self.assertLess(unchanged, write)
        self.assertIn("return true;", STORE[unchanged:write])

    def test_changed_cache_is_persisted(self):
        self.assertIn(
            "const bool saved = _storage.setBytes(STORAGE_KEY, &stored, sizeof(stored));",
            STORE,
        )

    def test_old_application_servers_are_removed(self):
        all_source = "\n".join(
            p.read_text(errors="ignore") for p in (ROOT / "src").rglob("*") if p.is_file()
        )
        for obsolete in ("BootstrapServer", "DiscoveryService",
                         "DISCOVER_FERMENTATION_SENSORS", "4210", "POST /connect"):
            self.assertNotIn(obsolete, all_source)


if __name__ == "__main__":
    unittest.main()
