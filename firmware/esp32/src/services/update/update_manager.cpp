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
static GitHubProvider* s_githubProvider = nullptr;
static Transport::ITransport* s_currentTransport = nullptr; // Transport layer instance
static Verification::IVerifier* s_currentVerifier = nullptr; // Verification layer instance (Phase 5)
static Installation::IInstaller* s_currentInstaller = nullptr; // Installation layer instance (Phase 6A)
static Reboot::RebootController* s_currentRebootController = nullptr; // Reboot orchestration layer instance (Phase 6C.1)
static BootValidation::BootValidationManager* s_currentBootValidationManager = nullptr; // Boot validation layer instance (Phase 6C.2)
static Acceptance::FirmwareAcceptanceManager* s_currentFirmwareAcceptanceManager = nullptr; // Firmware acceptance layer instance (Phase 6C.3)
static Rollback::RollbackManager* s_currentRollbackManager = nullptr; // Rollback layer instance (Phase 6D)
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

void registerGitHubProvider(GitHubProvider* provider) {
    s_githubProvider = provider;
    s_currentProvider = provider != nullptr ? static_cast<IUpdateProvider*>(provider)
                                            : static_cast<IUpdateProvider*>(&s_nullProvider);

    if (s_initialized && s_currentProvider) {
        s_currentProvider->begin();
    }

#ifdef OTA_PLATFORM_VALIDATION
    if (s_githubProvider) {
        LOG_INFO(LogTag::UPDATE, "GitHub provider registered successfully");
    }
#endif
}

GitHubProvider* githubProvider() {
    return s_githubProvider;
}

// Phase 5 verifier implementations
void registerVerifier(Verification::IVerifier* verifier) {
    s_currentVerifier = verifier;
    if (s_currentVerifier && !s_currentVerifier->isReady()) {
        s_currentVerifier->initialize();
    }
    
#ifdef OTA_PLATFORM_VALIDATION
    if (s_currentVerifier) {
        LOG_INFO(LogTag::UPDATE, "Verifier registered successfully");
    }
#endif
}

bool hasVerifier() {
    return s_currentVerifier != nullptr && s_currentVerifier->isReady();
}

const Verification::IVerifier* currentVerifier() {
    return s_currentVerifier;
}

// Phase 6A installer implementations
void registerInstaller(Installation::IInstaller* installer) {
    s_currentInstaller = installer;
    if (s_currentInstaller && !s_currentInstaller->isReady()) {
        s_currentInstaller->initialize();
    }
    
#ifdef OTA_PLATFORM_VALIDATION
    if (s_currentInstaller) {
        LOG_INFO(LogTag::UPDATE, "Installer registered successfully");
    }
#endif
}

bool hasInstaller() {
    return s_currentInstaller != nullptr && s_currentInstaller->isReady();
}

const Installation::IInstaller* currentInstaller() {
    return s_currentInstaller;
}

// Phase 6C.1 reboot controller registration
void registerRebootController(Reboot::RebootController* controller) {
    s_currentRebootController = controller;

#ifdef OTA_PLATFORM_VALIDATION
    if (s_currentRebootController) {
        LOG_INFO(LogTag::UPDATE, "Reboot controller registered successfully");
    }
#endif
}

bool hasRebootController() {
    return s_currentRebootController != nullptr;
}

Reboot::RebootController* currentRebootController() {
    return s_currentRebootController;
}

// Phase 6C.2 boot validation registration
void registerBootValidationManager(BootValidation::BootValidationManager* manager) {
    s_currentBootValidationManager = manager;

#ifdef OTA_PLATFORM_VALIDATION
    if (s_currentBootValidationManager) {
        LOG_INFO(LogTag::UPDATE, "Boot validation manager registered successfully");
    }
#endif
}

bool hasBootValidationManager() {
    return s_currentBootValidationManager != nullptr;
}

BootValidation::BootValidationManager* currentBootValidationManager() {
    return s_currentBootValidationManager;
}

// Phase 6C.3 firmware acceptance registration
void registerFirmwareAcceptanceManager(Acceptance::FirmwareAcceptanceManager* manager) {
    s_currentFirmwareAcceptanceManager = manager;

#ifdef OTA_PLATFORM_VALIDATION
    if (s_currentFirmwareAcceptanceManager) {
        LOG_INFO(LogTag::UPDATE, "Firmware acceptance manager registered successfully");
    }
#endif
}

Acceptance::FirmwareAcceptanceManager* currentFirmwareAcceptanceManager() {
    return s_currentFirmwareAcceptanceManager;
}

// Phase 6D rollback manager registration
void registerRollbackManager(Rollback::RollbackManager* manager) {
    s_currentRollbackManager = manager;

#ifdef OTA_PLATFORM_VALIDATION
    if (s_currentRollbackManager) {
        LOG_INFO(LogTag::UPDATE, "Rollback manager registered successfully");
    }
#endif
}

Rollback::RollbackManager* rollbackManager() {
    return s_currentRollbackManager;
}

VersionComparison compareVersions(uint32_t currentVersionCode, uint32_t remoteVersionCode) {
    return VersionCompare::compare(currentVersionCode, remoteVersionCode);
}

UpdateDecision checkForUpdate() {
    s_lastDecisionContext = s_currentProvider->checkForUpdate();

    if (s_lastDecisionContext.hasUpdateInfo) {
        s_latestUpdateInfo = s_lastDecisionContext.updateInfo;
    }

    if (s_lastDecisionContext.decision != UpdateDecisionType::UpdateAvailable &&
        s_lastDecisionContext.decision != UpdateDecisionType::UpdateRecommended &&
        s_lastDecisionContext.decision != UpdateDecisionType::UpdateRequired) {
        return {s_lastDecisionContext.decision};
    }

    UpdateError validation = UpdateValidator::validateUpdateInfo(s_latestUpdateInfo);
    if (validation.hasError()) {
        s_lastDecisionContext = UpdateDecisionContext::create(
            UpdateDecisionType::Error,
            "Update validation failed: " + validation.message());
        return {s_lastDecisionContext.decision};
    }

    UpdateError compatibility = UpdateValidator::checkCompatibility(
        s_latestUpdateInfo,
        s_firmwareInfo);
    if (compatibility.hasError()) {
        s_lastDecisionContext = UpdateDecisionContext::create(
            UpdateDecisionType::Error,
            "Compatibility check failed: " + compatibility.message());
        return {s_lastDecisionContext.decision};
    }

    VersionComparison versionCompare = UpdateValidator::compareVersion(
        s_latestUpdateInfo,
        s_firmwareInfo);

    if (versionCompare != VersionComparison::Newer) {
        s_lastDecisionContext = UpdateDecisionContext::create(
            UpdateDecisionType::NoUpdateAvailable,
            "Available version is not newer than current firmware");
        return {s_lastDecisionContext.decision};
    }
    
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
    if (s_currentRebootController) {
        LOG_INFO(LogTag::UPDATE, "Reboot controller state: %d",
                 static_cast<uint8_t>(s_currentRebootController->state()));
        LOG_INFO(LogTag::UPDATE, "Reboot pending: %s",
                 s_currentRebootController->hasPendingReboot() ? "Yes" : "No");
    }

    if (s_githubProvider) {
        const auto& diagnostics = s_githubProvider->diagnostics();
        LOG_INFO(LogTag::UPDATE, "GitHub provider state: %d",
                 static_cast<uint8_t>(s_githubProvider->state()));
        LOG_INFO(LogTag::UPDATE, "GitHub HTTP status: %d",
                 diagnostics.lastHttpStatus);
        LOG_INFO(LogTag::UPDATE, "GitHub retry count: %d",
                 diagnostics.retryCount);
        LOG_INFO(LogTag::UPDATE, "GitHub request duration: %u ms",
                 diagnostics.requestDurationMs);
    }
    if (s_currentBootValidationManager) {
        const auto& bootReport = s_currentBootValidationManager->report();
        LOG_INFO(LogTag::UPDATE, "Boot validation state: %d",
                 static_cast<uint8_t>(s_currentBootValidationManager->state()));
        LOG_INFO(LogTag::UPDATE, "Boot validation result: %d",
                 static_cast<uint8_t>(bootReport.result));
    }
    if (s_currentFirmwareAcceptanceManager) {
        const auto& acceptanceReport = s_currentFirmwareAcceptanceManager->report();
        LOG_INFO(LogTag::UPDATE, "Firmware acceptance state: %d",
                 static_cast<uint8_t>(s_currentFirmwareAcceptanceManager->state()));
        LOG_INFO(LogTag::UPDATE, "Firmware acceptance result: %d",
                 static_cast<uint8_t>(acceptanceReport.result));
    }
    if (s_currentRollbackManager) {
        const auto& rollbackReport = s_currentRollbackManager->report();
        LOG_INFO(LogTag::UPDATE, "Rollback state: %d",
                 static_cast<uint8_t>(s_currentRollbackManager->state()));
        LOG_INFO(LogTag::UPDATE, "Rollback result: %d",
                 static_cast<uint8_t>(rollbackReport.result));
        LOG_INFO(LogTag::UPDATE, "Rollback reason: %d",
                 static_cast<uint8_t>(rollbackReport.reason));
        LOG_INFO(LogTag::UPDATE, "Rollback possible/required/executed: %d/%d/%d",
                 rollbackReport.rollbackPossible,
                 rollbackReport.rollbackRequired,
                 rollbackReport.rollbackExecuted);
    }
    LOG_INFO(LogTag::UPDATE, "=====================================");
#endif
}

} // namespace UpdateManager
