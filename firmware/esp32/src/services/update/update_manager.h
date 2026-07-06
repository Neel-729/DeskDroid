#pragma once

#include <Arduino.h>
#include "update_state.h"
#include "version_compare.h"
#include "firmware_info.h"
#include "providers/update_provider.h"
#include "providers/null_provider.h"

namespace UpdateManager {
void begin();
void loop();
bool isInitialized();

const FirmwareInfo& firmwareInfo();
UpdateState state();
const IUpdateProvider& currentProvider();

VersionComparison compareVersions(uint32_t currentVersionCode, uint32_t remoteVersionCode);
}