from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]
HTTP = (ROOT / "src/network/ConfigHttpServer.cpp").read_text()
PAGE = (ROOT / "src/network/ConfigPage.h").read_text()
STORE = (ROOT / "src/storage/SensorConfigStore.cpp").read_text()
SERVICE = (ROOT / "src/config/SensorConfigService.cpp").read_text()
PRESSURE = (ROOT / "src/sensors/PressureSensor.cpp").read_text()
PRESSURE_H = (ROOT / "include/sensors/PressureSensor.h").read_text()
TEMP_H = (ROOT / "include/sensors/TemperatureSensor.h").read_text()

class SensorConfigTests(unittest.TestCase):
    def test_defaults_and_validation_are_host_tested(self):
        source = r'''
#include "config/SensorConfig.h"
#include <cassert>
#include <limits>
int main() {
 auto c=SensorConfig::defaults(); const char* e=nullptr;
 assert(c.eventDiagnosticsEnabled && !c.rawPressureDiagnosticsEnabled && c.validate(e));
 c.minDurationMs=4000; c.maxDurationMs=3000; assert(!c.validate(e));
 c=SensorConfig::defaults(); c.noiseFactor=std::numeric_limits<float>::quiet_NaN(); assert(!c.validate(e));
 c=SensorConfig::defaults(); c.releaseFactor=std::numeric_limits<float>::infinity(); assert(!c.validate(e));
}'''
        with tempfile.TemporaryDirectory() as d:
            p=Path(d); (p/'test.cpp').write_text(source)
            subprocess.run(['g++','-std=c++17','-I',str(ROOT/'include'),str(p/'test.cpp'),str(ROOT/'src/config/SensorConfig.cpp'),'-o',str(p/'t')],check=True)
            subprocess.run([str(p/'t')],check=True)

    def test_single_versioned_verified_record(self):
        self.assertIn('"sensor_config_v1"', STORE)
        self.assertIn('MAGIC', STORE); self.assertIn('VERSION', STORE)
        self.assertIn('memcmp(&written, &read', STORE)

    def test_service_only_applies_after_verified_save_and_skips_identical(self):
        self.assertLess(SERVICE.index('saveAndVerify(config)'), SERVICE.index('_config = config'))
        self.assertIn('config == _config', SERVICE)

    def test_http_state_guards_and_protected_fields(self):
        self.assertIn('MeasurementState::IDLE', HTTP)
        self.assertIn('409', HTTP)
        for field in ('deviceId','ssid','password','sequence','ackTimeout'):
            self.assertIn(f'"{field}"', HTTP)

    def test_api_never_serializes_wifi_secrets(self):
        section=HTTP[HTTP.index('String configJson'):HTTP.index('const char* measurementStateName')]
        self.assertNotIn('ssid',section.lower()); self.assertNotIn('password',section.lower())

    def test_page_is_self_contained_and_defaults_do_not_post(self):
        self.assertNotIn('https://', PAGE); self.assertNotIn('http://', PAGE)
        self.assertIn("defaults.onclick=()=>fill(defaults)", PAGE)
        self.assertIn("fetch('/api/config'", PAGE)

    def test_live_status_exposes_sensor_ids_and_errors(self):
        self.assertIn('GET /api/status', HTTP)
        self.assertIn('ambientId', HTTP)
        self.assertIn('beerId', HTTP)
        self.assertIn('errors', HTTP)
        self.assertIn("fetch('/api/status'", PAGE)
        self.assertIn('ambientSensorId', PAGE)
        self.assertIn('beerSensorId', PAGE)

    def test_pressure_runtime_errors_are_tracked_for_status_page(self):
        self.assertIn('_lastReadError = sample.error', PRESSURE)
        self.assertIn('getLastReadErrorName', PRESSURE)
        self.assertIn('hasReadAttempted', PRESSURE_H)
        self.assertIn('isLastReadValid', PRESSURE_H)

    def test_temperature_ids_and_cached_health_are_read_only(self):
        self.assertIn('getAmbientSensorId() const', TEMP_H)
        self.assertIn('getBeerSensorId() const', TEMP_H)
        self.assertIn('isLastMeasurementValid() const', TEMP_H)

    def test_boolean_parser_is_strict(self):
        self.assertIn('== "true"', HTTP); self.assertIn('== "false"', HTTP)

if __name__ == '__main__': unittest.main()
