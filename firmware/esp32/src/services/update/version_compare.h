#pragma once

#include <Arduino.h>

enum class VersionComparison {
    Older,
    Equal,
    Newer
};

namespace VersionCompare {
VersionComparison compare(uint32_t currentVersionCode, uint32_t remoteVersionCode);
}