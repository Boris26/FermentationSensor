from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NanoEsp32PortTests(unittest.TestCase):
    def test_only_nano_esp32_platformio_target(self):
        pio = (ROOT / "platformio.ini").read_text()
        self.assertIn("[env:nanoesp32]", pio)
        self.assertIn("board = arduino_nano_esp32", pio)
        self.assertIn("platform = espressif32", pio)
        self.assertNotIn("raspberrypi", pio.lower())

    def test_native_wifi_and_nvs(self):
        sources = "\n".join(p.read_text(errors="ignore") for p in
            list((ROOT / "include").rglob("*")) + list((ROOT / "src").rglob("*")) if p.is_file())
        self.assertIn("#include <WiFi.h>", sources)
        self.assertIn("#include <Preferences.h>", sources)
        self.assertIn("Preferences _preferences", sources)

    def test_identity_and_sequence_remain_persistent(self):
        identity = (ROOT / "src/device/DeviceIdentity.cpp").read_text()
        sequence = (ROOT / "src/network/MeasurementSequenceAllocator.cpp").read_text()
        self.assertIn("saveConfiguration", identity)
        self.assertIn("loadConfiguration", identity)
        self.assertIn("esp_random()", identity)
        self.assertIn("_store.save", sequence)

    def test_documented_pin_mapping_and_led_recovery(self):
        readme = (ROOT / "README.md").read_text()
        for pin in ("D2", "D3", "D4", "D5", "D6", "A4/SDA", "A5/SCL"):
            self.assertIn(pin, readme)
        status = (ROOT / "src/app/StatusController.cpp").read_text()
        self.assertIn("_measurementDisplayAvailable", status)
        self.assertIn("LedMode::RUNNING_BLINK", status)

if __name__ == "__main__":
    unittest.main()
