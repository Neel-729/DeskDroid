#pragma once

#include <Arduino.h>
#include "esp_partition.h"

struct FirmwareInfo {
    const char* firmwareName;
    const char* firmwareVersion;
    uint32_t versionCode;
    const char* buildDate;
    const char* buildTime;
    const char* firmwareChannel;
    const char* hardwareRevision;
    uint32_t protocolVersion;
    bool otaSupported;
    char runningPartitionName[16];
    char bootPartitionName[16];
};

namespace FirmwareMetadata {
void populate(FirmwareInfo& info);
const FirmwareInfo& getCurrent();
}