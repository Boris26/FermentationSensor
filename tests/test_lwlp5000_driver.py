"""Host-side tests for the controlled SEN0343/LWLP5000 driver."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).parents[1]
HEADER = (ROOT / "include/sensors/Lwlp5000Driver.h").read_text()
SOURCE = (ROOT / "src/sensors/Lwlp5000Driver.cpp").read_text()
PLATFORMIO = (ROOT / "platformio.ini").read_text()


class Lwlp5000DriverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._temporary_directory = tempfile.TemporaryDirectory()
        temporary = Path(cls._temporary_directory.name)
        (temporary / "Arduino.h").write_text(textwrap.dedent(r"""
            #pragma once
            #include <stdint.h>
            inline void delay(unsigned long) {}
        """))
        (temporary / "Wire.h").write_text(textwrap.dedent(r"""
            #pragma once
            #include <stddef.h>
            #include <stdint.h>
            #include <vector>

            class TwoWire {
            public:
                void begin() { began = true; }
                void beginTransmission(uint8_t address) { lastAddress = address; tx.clear(); }
                size_t write(const uint8_t* data, size_t size) {
                    if (failWrite) return 0;
                    tx.assign(data, data + size);
                    return size;
                }
                uint8_t endTransmission() { return failTransmission ? 4 : 0; }
                uint8_t requestFrom(uint8_t address, uint8_t count) {
                    lastAddress = address;
                    readIndex = 0;
                    return shortRead ? count - 1 : static_cast<uint8_t>(response.size());
                }
                int available() { return static_cast<int>(response.size() - readIndex); }
                int read() { return response.at(readIndex++); }

                bool began = false;
                bool failWrite = false;
                bool failTransmission = false;
                bool shortRead = false;
                uint8_t lastAddress = 0xff;
                size_t readIndex = 0;
                std::vector<uint8_t> tx;
                std::vector<uint8_t> response;
            };

            extern TwoWire Wire;
        """))
        harness = temporary / "lwlp5000_driver_test.cpp"
        harness.write_text(textwrap.dedent(r"""
            #include "sensors/Lwlp5000Driver.h"
            #include <Wire.h>
            #include <cassert>
            #include <cmath>

            TwoWire Wire;

            bool closeTo(float actual, float expected, float tolerance = 0.07f)
            {
                return std::fabs(actual - expected) <= tolerance;
            }

            std::vector<uint8_t> responseFor(uint16_t pressureRaw, uint8_t status = 0)
            {
                const uint16_t encodedPressure = pressureRaw << 2;
                const uint32_t encodedTemperature = 4096UL << 11;
                return {status,
                    static_cast<uint8_t>(encodedPressure >> 8),
                    static_cast<uint8_t>(encodedPressure), 0,
                    static_cast<uint8_t>(encodedTemperature >> 16),
                    static_cast<uint8_t>(encodedTemperature >> 8),
                    static_cast<uint8_t>(encodedTemperature)};
            }

            int main()
            {
                assert(Lwlp5000Driver::decodePressureRaw(0x80, 0x00) == 8192);
                assert(closeTo(Lwlp5000Driver::pressureRawToPa(0), -500.0f));
                assert(closeTo(Lwlp5000Driver::pressureRawToPa(8192), 0.0f));
                assert(closeTo(Lwlp5000Driver::pressureRawToPa(16383), 500.0f));
                assert(closeTo(Lwlp5000Driver::pressureRawToPa(4096), -250.0f));
                assert(closeTo(Lwlp5000Driver::pressureRawToPa(12288), 250.0f));
                assert(closeTo(Lwlp5000Driver::pressureRawToPa(9799), 98.07f));
                assert(!closeTo(Lwlp5000Driver::pressureRawToPa(9799), 117.7f));

                TwoWire wire;
                Lwlp5000Driver driver(wire);
                wire.response = responseFor(8765); // approximately +35 Pa
                assert(driver.begin());
                assert(wire.began);
                assert(wire.lastAddress == 0x00);
                // begin() only probes I2C; it neither commands nor reads a sample.
                assert(wire.tx.empty());

                Lwlp5000Sample first = driver.read();
                assert(first.valid && closeTo(first.pressurePa, 35.0f));
                assert(closeTo(first.temperatureC, 2.5f));
                assert((wire.tx == std::vector<uint8_t>{0xAA, 0x00, 0x80}));
                Lwlp5000Sample second = driver.read();
                assert(second.valid && closeTo(second.pressurePa, 35.0f));

                wire.response = responseFor(8192, 0x20);
                Lwlp5000Sample busy = driver.read();
                assert(!busy.valid && busy.status == 0x20);
                wire.shortRead = true;
                assert(!driver.read().valid);

                TwoWire missingWire;
                missingWire.failTransmission = true;
                Lwlp5000Driver missing(missingWire);
                assert(!missing.begin());
                assert(!missing.read().valid);
                return 0;
            }
        """))
        cls.executable = temporary / "lwlp5000_driver_test"
        subprocess.run([
            "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-I", str(temporary), "-I", str(ROOT / "include"),
            str(harness), str(ROOT / "src/sensors/Lwlp5000Driver.cpp"),
            "-o", str(cls.executable),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls._temporary_directory.cleanup()

    def test_conversion_protocol_and_no_boot_tare(self):
        subprocess.run([str(self.executable)], check=True)

    def test_pressure_range_cannot_regress_to_600_pa(self):
        self.assertIn("PRESSURE_MIN_PA = -500.0f", HEADER)
        self.assertIn("PRESSURE_MAX_PA = 500.0f", HEADER)
        self.assertNotIn("600", HEADER + SOURCE)

    def test_external_dfrobot_dependency_is_removed(self):
        self.assertNotIn("DFRobot_LWLP", PLATFORMIO + HEADER + SOURCE)


if __name__ == "__main__":
    unittest.main()
