#pragma once

#include <Arduino.h>
#include "update_state.h"
#include "version_compare.h"
#include "firmware_info.h"
#include "providers/update_provider.h"
#include "providers/null_provider.h"
#include "models/update_info.h"
#include "models/update_decision.h"

namespace UpdateManager {
void begin();
void loop();
bool isInitialized();

const FirmwareInfo& firmwareInfo();
UpdateState state();
const IUpdateProvider& currentProvider();

VersionComparison compareVersions(uint32_t currentVersionCode, uint32_t remoteVersionCode);

// Phase 3 update methods - updated public API
UpdateDecision checkForUpdate();
const UpdateInfo& getLatestUpdateInfo(); // Returns const reference to internal static data
void setLatestUpdateInfo(const UpdateInfo& info);
UpdateDecisionContext getUpdateDecisionContext(); // Returns copy of internal context

// Diagnostic method (only active when OTA_PLATFORM_VALIDATION is enabled)
void printUpdateStatus();
}