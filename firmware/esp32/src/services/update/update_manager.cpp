#include "update_manager.h"
#include "../../core/logging.h"
#include "firmware_info.h"
#include "version_compare.h"
#include "models/update_validator.h"
#include "models/update_decision.h"
#include "transport/itransport.h"

static bool s_initialized = false;
static UpdateState s_currentState = UpdateState::Idle;
static FirmwareInfo s_firmwareInfo;
static NullUpdateProvider s_nullProvider;
static IUpdateProvider* s_currentProvider = &s_nullProvider;
static Transport::ITransport* s_currentTransport = nullptr; // Transport layer instance
static UpdateInfo s_latestUpdateInfo; // Cached latest available update information
static UpdateDecisionContext s_lastDecisionContext; // Cached last decision context

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
    
    // Initialize transport if available
    if (s_currentTransport) {
        if (s_currentTransport->initialize()) {
            LOG_INFO(LogTag::UPDATE, "Transport: %s initialized", s_currentTransport->transportName());
        } else {
            LOG_WARN(LogTag::UPDATE, "Transport initialization failed: %d", static_cast<uint8_t>(s_currentTransport->lastError()));
        }
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
    
    // Transport doesn't need a loop in current passive implementation,
    // but structure is in place for future transport implementations that might need it
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

UpdateDecision checkForUpdate() {
    // First evaluate the full context to store it
    s_lastDecisionContext = [&]() {
        // If no update info has been set, return no update available
        if (!s_latestUpdateInfo.isValid()) {
            return UpdateDecisionContext::create(
                UpdateDecisionType::NoUpdateAvailable,
                "No valid update information available"
            );
        }
        
        // Validate the update info first
        UpdateError validation = UpdateValidator::validateUpdateInfo(s_latestUpdateInfo);
        if (validation.hasError()) {
            return UpdateDecisionContext::create(
                UpdateDecisionType::Error,
                "Update validation failed: " + validation.message()
            );
        }
        
        // Check compatibility with current hardware/firmware
        UpdateError compatibility = UpdateValidator::checkCompatibility(
            s_latestUpdateInfo, 
            s_firmwareInfo
        );
        if (compatibility.hasError()) {
            return UpdateDecisionContext::create(
                UpdateDecisionType::Error,
                "Compatibility check failed: " + compatibility.message()
            );
        }
        
        // Compare versions to see if this is actually newer
        VersionComparison versionCompare = UpdateValidator::compareVersion(
            s_latestUpdateInfo, 
            s_firmwareInfo
        );
        
        if (versionCompare != VersionComparison::Newer) {
            return UpdateDecisionContext::create(
                UpdateDecisionType::NoUpdateAvailable,
                "Available version is not newer than current firmware"
            );
        }
        
        // If all checks pass, the update is available
        // In a real implementation, this could evaluate severity to return
        // UpdateRecommended or UpdateRequired based on release metadata
        return UpdateDecisionContext::withUpdateInfo(
            UpdateDecisionType::UpdateAvailable,
            "New firmware version " + s_latestUpdateInfo.version + " is available",
            s_latestUpdateInfo
        );
    }();
    
    // Return just the decision type as the primary result
    return {s_lastDecisionContext.decision};
}

const UpdateInfo& getLatestUpdateInfo() {
    return s_latestUpdateInfo;
}

void setLatestUpdateInfo(const UpdateInfo& info) {
    s_latestUpdateInfo = info;
    LOG_INFO(LogTag::UPDATE, "Update information set: v%s (%d)", 
              info.version.c_str(), info.numericVersion);
}

UpdateDecisionContext getUpdateDecisionContext() {
    return s_lastDecisionContext;
}

const Transport::ITransport* currentTransport() {
    return s_currentTransport;
}

void registerTransport(Transport::ITransport* transport) {
    // If we have an existing transport, shutdown first
    if (s_currentTransport && s_currentTransport != transport) {
        s_currentTransport->shutdown();
    }
    
    s_currentTransport = transport;
    
    // If we're already initialized, initialize the new transport immediately
    if (s_initialized && s_currentTransport) {
        if (!s_currentTransport->initialize()) {
            LOG_WARN(LogTag::UPDATE, "Late transport initialization failed: %d", 
                     static_cast<uint8_t>(s_currentTransport->lastError()));
        } else {
            LOG_INFO(LogTag::UPDATE, "Late transport initialized: %s", 
                     s_currentTransport->transportName());
        }
    }
}

bool isTransportAvailable() {
    return s_currentTransport != nullptr;
}



void printUpdateStatus() {
#ifdef OTA_PLATFORM_VALIDATION
    LOG_INFO(LogTag::UPDATE, "=== OTA Update Status Diagnostics ===");
    LOG_INFO(LogTag::UPDATE, "Current firmware: v%s (%d)", 
             s_firmwareInfo.firmwareVersion, s_firmwareInfo.versionCode);
    LOG_INFO(LogTag::UPDATE, "Hardware revision: %s", 
             s_firmwareInfo.hardwareRevision);
    
    // Print transport status if available
    if (s_currentTransport) {
        LOG_INFO(LogTag::UPDATE, "Transport: %s", s_currentTransport->transportName());
        LOG_INFO(LogTag::UPDATE, "  State: %d", static_cast<uint8_t>(s_currentTransport->state()));
        LOG_INFO(LogTag::UPDATE, "  Busy: %s", s_currentTransport->isBusy() ? "Yes" : "No");
        LOG_INFO(LogTag::UPDATE, "  Supports resume: %s", s_currentTransport->supportsResume() ? "Yes" : "No");
        LOG_INFO(LogTag::UPDATE, "  Supports streaming: %s", s_currentTransport->supportsStreaming() ? "Yes" : "No");
        
        if (s_currentTransport->isBusy()) {
            const auto& progress = s_currentTransport->progress();
            LOG_INFO(LogTag::UPDATE, "  Progress: %u/%u bytes (%u%%)", 
                     progress.bytesTransferred, progress.expectedSize, progress.percentage());
        }
    }
    
    if (s_latestUpdateInfo.isValid()) {
        LOG_INFO(LogTag::UPDATE, "Latest available update: v%s (%d)", 
                 s_latestUpdateInfo.version.c_str(), s_latestUpdateInfo.numericVersion);
        LOG_INFO(LogTag::UPDATE, "Update download URL: %s", 
                 s_latestUpdateInfo.downloadUrl.c_str());
        LOG_INFO(LogTag::UPDATE, "Update file size: %u bytes", 
                 static_cast<unsigned int>(s_latestUpdateInfo.fileSize));
    } else {
        LOG_INFO(LogTag::UPDATE, "No valid update information currently stored");
    }
    
    LOG_INFO(LogTag::UPDATE, "Last update decision: %s", 
             updateDecisionTypeToString(s_lastDecisionContext.decision));
    LOG_INFO(LogTag::UPDATE, "Decision reason: %s", 
             s_lastDecisionContext.reason.c_str());
    LOG_INFO(LogTag::UPDATE, "=====================================");
#endif
}

} // namespace UpdateManager