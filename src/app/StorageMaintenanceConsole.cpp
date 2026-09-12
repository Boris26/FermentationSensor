#include <Arduino.h>
#include <cstring>

#include "app/StorageMaintenanceConsole.h"
#include "storage/StorageMaintenance.h"

constexpr char StorageMaintenanceConsole::COMMAND[];

StorageMaintenanceConsole::StorageMaintenanceConsole(
    StorageMaintenance& maintenance
) : _maintenance(maintenance)
{
}

void StorageMaintenanceConsole::announceAvailable() const
{
    Serial.println("STORAGE_MAINTENANCE_AVAILABLE");
    Serial.println("Send COMPACT_STORAGE at any time while storage error is active");
}

void StorageMaintenanceConsole::resetCommandBuffer()
{
    _commandLength = 0;
    _commandBuffer[0] = '\0';
    _commandOverflow = false;
}

void StorageMaintenanceConsole::update(bool measurementSequenceReady)
{
    if (measurementSequenceReady) return;
    while (Serial.available()) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\r') continue;
        if (character != '\n') {
            if (_commandLength < sizeof(_commandBuffer) - 1) {
                _commandBuffer[_commandLength++] = character;
                _commandBuffer[_commandLength] = '\0';
            } else {
                _commandOverflow = true;
            }
            continue;
        }
        const bool matches = !_commandOverflow &&
            strcmp(_commandBuffer, COMMAND) == 0;
        resetCommandBuffer();
        if (!matches) continue;
        if (_maintenance.compactPersistentStorage()) {
            Serial.println("STORAGE_MAINTENANCE_RESTARTING");
            Serial.flush();
            delay(50);
            NVIC_SystemReset();
        }
        return;
    }
}
