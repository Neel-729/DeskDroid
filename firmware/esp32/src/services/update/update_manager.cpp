#include "update_manager.h"
#include "../../core/logging.h"
#include "firmware_info.h"
#include "version_compare.h"

static bool s_initialized = false;
static UpdateState s_currentState = UpdateState::Idle;
static FirmwareInfo s_firmwareInfo;
static NullUpdateProvider s_nullProvider;
static IUpdateProvider* s_currentProvider = &s_nullProvider;

namespace UpdateManager {
void begin() {
    if (s_initialized) {
        return;
    }

    // Populate firmware metadata
    FirmwareMetadata::populate(s_firmwareInfo);
    
    // Set initial state
    s_currentState = UpdateState::Idle;
    
    // Initialize the provider
    if (s_currentProvider) {
        s_currentProvider->begin();
        LOG_INFO(LogTag::UPDATE, "Provider: %s", s_currentProvider->providerName());
    }
    
    s_initialized = true;
    
    // Log initialization - single optional startup message
    LOG_INFO(LogTag::UPDATE, "OTA engine initialized");
}

void loop() {
    // Call provider loop if available
    if (s_currentProvider) {
        s_currentProvider->loop();
    }
}

bool isInitialized() {
    return s_initialized;
}

const FirmwareInfo& firmwareInfo() {
    return s_firmwareInfo;
}

UpdateState state() {
    return s_currentState;
}

const IUpdateProvider& currentProvider() {
    return *s_currentProvider;
}

VersionComparison compareVersions(uint32_t currentVersionCode, uint32_t remoteVersionCode) {
    return VersionCompare::compare(currentVersionCode, remoteVersionCode);
}
}