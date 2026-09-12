#pragma once

#include <stddef.h>

class StorageMaintenance;

class StorageMaintenanceConsole
{
public:
    explicit StorageMaintenanceConsole(StorageMaintenance& maintenance);
    void announceAvailable() const;
    void update(bool measurementSequenceReady);

private:
    void resetCommandBuffer();
    static constexpr char COMMAND[] = "COMPACT_STORAGE";
    StorageMaintenance& _maintenance;
    char _commandBuffer[sizeof(COMMAND)] = {};
    size_t _commandLength = 0;
    bool _commandOverflow = false;
};
