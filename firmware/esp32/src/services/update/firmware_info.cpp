#include "firmware_info.h"
#include "esp_ota_ops.h"

static FirmwareInfo s_currentInfo;

namespace FirmwareMetadata {
void populate(FirmwareInfo& info) {
    // Populate basic firmware metadata - these can be configured via build flags
    info.firmwareName = "DeskDroid";
    info.firmwareVersion = __DATE__;
    info.versionCode = 1; // Initial version code, can be incremented in future builds
    info.buildDate = __DATE__;
    info.buildTime = __TIME__;
    info.firmwareChannel = "stable";
    info.hardwareRevision = "1.0";
    info.protocolVersion = 1;
    info.otaSupported = true;

    // Get running and boot partition information from ESP-IDF
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        strncpy(info.runningPartitionName, running->label, sizeof(info.runningPartitionName) - 1);
        info.runningPartitionName[sizeof(info.runningPartitionName) - 1] = '\0';
    }

    const esp_partition_t* boot = esp_ota_get_boot_partition();
    if (boot) {
        strncpy(info.bootPartitionName, boot->label, sizeof(info.bootPartitionName) - 1);
        info.bootPartitionName[sizeof(info.bootPartitionName) - 1] = '\0';
    }
}

const FirmwareInfo& getCurrent() {
    return s_currentInfo;
}
}