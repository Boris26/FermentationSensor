# Persistent storage on ESP32 NVS

`FlashStorage` is backed by the ESP32 Arduino `Preferences` API and opens the read/write namespace `fermsensor`. The public storage methods remain the single persistence boundary for credentials, device identity, temperature assignments, gateway cache, sensor configuration and measurement-sequence state.

NVS keys have a 15-character limit. Short application keys are stored unchanged;
the three longer keys have explicit, collision-free aliases:

| Application key | NVS key |
|---|---|
| `temperature_config` | `temp_config_v1` |
| `measurement_sequence_v1` | `measure_seq_v1` |
| `sensor_config_v1` | `sensor_cfg_v1` |

Unknown oversized keys are rejected. Binary reads validate the exact stored
length. Writes are successful only when `Preferences` reports the complete
requested length.

`factoryReset()`/`clearAll()` clear the namespace. `clearWifi()` removes only credential records. No data is imported from older hardware: a Nano ESP32 is provisioned as a new physical device.

The device UUID is created once and written as part of `device_config`. Later boots validate and load that record. Measurement sequence allocation remains reserve-before-use: the next block boundary is persisted before values from a block can be returned, so a reboot may skip values but never repeats an allocated range.
