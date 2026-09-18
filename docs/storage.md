# Persistent storage on ESP32 NVS

`FlashStorage` is backed by the ESP32 Arduino `Preferences` API and opens the read/write namespace `fermsensor`. The public storage methods remain the single persistence boundary for credentials, device identity, temperature assignments, gateway cache, sensor configuration and measurement-sequence state.

NVS keys have a 15-character limit. `FlashStorage` therefore maps each descriptive application key deterministically to an FNV-1a-derived key while Store classes continue using their established names. Binary reads validate the exact stored length. Writes are successful only when `Preferences` reports the complete requested length.

`factoryReset()`/`clearAll()` clear the namespace. `clearWifi()` removes only credential records. No data is imported from older hardware: a Nano ESP32 is provisioned as a new physical device.

The device UUID is created once and written as part of `device_config`. Later boots validate and load that record. Measurement sequence allocation remains reserve-before-use: the next block boundary is persisted before values from a block can be returned, so a reboot may skip values but never repeats an allocated range.
