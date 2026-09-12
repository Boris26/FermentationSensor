from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]
HTTP = (ROOT / "src/network/ConfigHttpServer.cpp").read_text()
PAGE = (ROOT / "src/network/ConfigPage.h").read_text()
STORE = (ROOT / "src/storage/SensorConfigStore.cpp").read_text()
SERVICE = (ROOT / "src/config/SensorConfigService.cpp").read_text()

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
        section=HTTP[HTTP.index('String configJson'):HTTP.index('ConfigHttpServer::ConfigHttpServer')]
        self.assertNotIn('ssid',section.lower()); self.assertNotIn('password',section.lower())

    def test_page_is_self_contained_and_defaults_do_not_post(self):
        self.assertNotIn('https://', PAGE); self.assertNotIn('http://', PAGE)
        self.assertIn("defaults.onclick=()=>fill(defaults)", PAGE)
        self.assertIn("fetch('/api/config'", PAGE)

    def test_boolean_parser_is_strict(self):
        self.assertIn('== "true"', HTTP); self.assertIn('== "false"', HTTP)

if __name__ == '__main__': unittest.main()
