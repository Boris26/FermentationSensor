# Dedicated persistent storage

The Arduino Nano RP2040 Connect has a 16 MiB QSPI flash, while the PlatformIO
board definition limits the application image to 2 MiB. Mbed's default global
`/kv/` TDBStore occupies only the final 8 KiB of flash. That small log-structured
store can reach `MBED_ERROR_MEDIA_FULL` after enough rewrites even when the live
payload is tiny.

FermentationSensor therefore uses its own 64 KiB TDBStore immediately below the
legacy Mbed `/kv/` region. The location is derived at runtime from
`kv_get_default_flash_addresses()` rather than hard-coded. Before opening the
store, the firmware compares its start address with the linker symbol
`__flash_binary_end` and refuses initialization if the two regions would overlap.

On the first boot after this change, the firmware copies and verifies the known
application records from the old `/kv/` store, including device identity, WiFi,
temperature-sensor assignments, gateway cache, and the persistent measurement
sequence state. Only after a layout marker has been written to the new store is
the legacy `/kv/` store reset. No credential values are printed during migration.

Expected boot diagnostics include:

```text
APP_KV_START,0x...
APP_KV_SIZE,65536
LEGACY_KV_START,0x...
LEGACY_KV_SIZE,8192
FLASH_STORAGE_MIGRATION_START
FLASH_STORAGE_MIGRATED,<key>,<size>
FLASH_STORAGE_MIGRATION_SUCCESS
FLASH_STORAGE_LEGACY_CLEANUP_OK
```

Subsequent boots skip the migration and use the 64 KiB application store directly.
The `COMPACT_STORAGE` maintenance command still operates only on the dedicated
application store; a factory reset clears the legacy store first and then the
application store so stale configuration cannot be imported again after a partial
reset.
