"""Host-side behavioural tests for the complete measurement LED lifecycle."""

from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[1]


class StatusLedLifecycleTests(unittest.TestCase):
    def test_running_long_runtime_rollover_pause_and_sensor_override(self):
        with tempfile.TemporaryDirectory() as temporary:
            stubs = Path(temporary)
            (stubs / "network").mkdir()
            (stubs / "Arduino.h").write_text(textwrap.dedent(r"""
                #pragma once
                #include <stdint.h>
                #define HIGH 1
                #define LOW 0
                #define OUTPUT 1
                extern uint32_t fakeMillis;
                extern int pinValues[16];
                inline uint32_t millis() { return fakeMillis; }
                inline void pinMode(uint8_t, int) {}
                inline void digitalWrite(uint8_t pin, int value) { pinValues[pin] = value; }
                struct FakeSerial {
                    template <typename T> void print(const T&) {}
                    template <typename T> void println(const T&) {}
                };
                extern FakeSerial Serial;
            """))
            (stubs / "network/NetworkManager.h").write_text(textwrap.dedent("""
                #pragma once
                class NetworkManager {
                public:
                    bool isConnected() const { return true; }
                };
            """))
            (stubs / "network/ServerClient.h").write_text(textwrap.dedent("""
                #pragma once
                class ServerClient {
                public:
                    bool isConnected() const { return true; }
                    bool isRegistered() const { return true; }
                };
            """))
            harness = stubs / "status_led_harness.cpp"
            harness.write_text(textwrap.dedent(r"""
                #include <assert.h>
                #include <stdint.h>
                #include "Arduino.h"
                #include "app/StatusController.h"
                #include "network/NetworkManager.h"
                #include "network/ServerClient.h"
                #include "output/ErrorLed.h"
                #include "output/StatusLed.h"

                uint32_t fakeMillis = 0;
                int pinValues[16] = {};
                FakeSerial Serial;

                static void advance(StatusController& status, uint32_t delta) {
                    fakeMillis += delta;
                    status.update(true, true);
                }

                int main() {
                    NetworkManager network;
                    ServerClient server;
                    StatusLed statusLed(2);
                    StatusLed sessionLed(6);
                    ErrorLed errorLed(5);
                    StatusController status(statusLed, sessionLed, errorLed, network, server);
                    status.begin();
                    status.update(true, true);

                    status.showMeasurementState(MeasurementState::RUNNING);
                    assert(pinValues[2] == LOW && pinValues[6] == HIGH);
                    advance(status, 1000);
                    assert(pinValues[6] == LOW);
                    advance(status, 1000);
                    assert(pinValues[6] == HIGH);

                    // Large timestamps (1h, 3h, 6h, 12h) must never stop blinking.
                    const uint32_t checkpoints[] = {1u, 3u, 6u, 12u};
                    for (uint32_t hours : checkpoints) {
                        fakeMillis = hours * 60u * 60u * 1000u;
                        int before = pinValues[6];
                        status.update(true, true);
                        assert(pinValues[6] != before);
                    }

                    status.showMeasurementState(MeasurementState::PAUSED);
                    assert(pinValues[2] == HIGH && pinValues[6] == HIGH);
                    status.showMeasurementState(MeasurementState::RUNNING);
                    assert(pinValues[2] == LOW && pinValues[6] == HIGH);
                    advance(status, 1000);
                    assert(pinValues[6] == LOW);

                    // Storage/network/backend errors have a dedicated LED and
                    // must not override the effective measurement display.
                    status.update(false, true);
                    advance(status, 1000);
                    assert(pinValues[6] == HIGH);

                    // A transient sensor override turns both measurement LEDs off,
                    // then restores cached RUNNING without a session transition.
                    status.update(true, false);
                    assert(pinValues[2] == LOW && pinValues[6] == LOW);
                    status.update(true, true);
                    assert(pinValues[2] == LOW && pinValues[6] == HIGH);
                    advance(status, 1000);
                    assert(pinValues[6] == LOW);

                    // Unsigned subtraction remains correct across millis() wrap.
                    fakeMillis = UINT32_MAX - 400u;
                    status.showMeasurementState(MeasurementState::PAUSED);
                    status.showMeasurementState(MeasurementState::RUNNING);
                    fakeMillis = 700u;
                    status.update(true, true);
                    assert(pinValues[6] == LOW);
                    return 0;
                }
            """))
            executable = stubs / "status_led_harness"
            subprocess.run([
                "g++", "-std=c++11", f"-I{stubs}", f"-I{ROOT / 'include'}",
                str(harness), str(ROOT / "src/app/StatusController.cpp"),
                str(ROOT / "src/output/StatusLed.cpp"),
                str(ROOT / "src/output/ErrorLed.cpp"), "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
