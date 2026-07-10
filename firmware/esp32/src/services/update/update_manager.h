#pragma once

#include <Arduino.h>
#include "update_state.h"
#include "version_compare.h"
#include "firmware_info.h"
#include "providers/update_provider.h"
#include "providers/null_provider.h"
#include "providers/github_provider.h"
#include "models/update_info.h"
#include "models/update_decision.h"
#include "transport/itransport.h"
#include "transport/http_transport.h"
#include "verification/iverifier.h"
#include "installer/iinstaller.h"
#include "reboot/reboot_controller.h"
#include "boot/boot_validation_manager.h"
#include "acceptance/firmware_acceptance_manager.h"
#include "rollback/rollback_manager.h"

namespace UpdateManager {
void begin();
void loop();
bool isInitialized();

const FirmwareInfo& firmwareInfo();
UpdateState state();
const IUpdateProvider& currentProvider();
const Transport::ITransport* currentTransport(); // Get current transport instance

// Provider registration
void registerGitHubProvider(GitHubProvider* provider);
GitHubProvider* githubProvider();

// Transport registration
void registerTransport(Transport::ITransport* transport);
bool isTransportAvailable();

// Verifier registration (Phase 5)
void registerVerifier(Verification::IVerifier* verifier);
bool hasVerifier();
const Verification::IVerifier* currentVerifier(); // Get current verifier instance

// Installer registration (Phase 6A)
void registerInstaller(Installation::IInstaller* installer);
bool hasInstaller();
const Installation::IInstaller* currentInstaller(); // Get current installer instance

// Reboot controller registration (Phase 6C.1)
void registerRebootController(Reboot::RebootController* controller);
bool hasRebootController();
Reboot::RebootController* currentRebootController(); // Get current reboot controller instance

// Boot validation registration (Phase 6C.2)
void registerBootValidationManager(BootValidation::BootValidationManager* manager);
bool hasBootValidationManager();
BootValidation::BootValidationManager* currentBootValidationManager(); // Get current boot validation manager instance

// Firmware acceptance registration (Phase 6C.3)
void registerFirmwareAcceptanceManager(Acceptance::FirmwareAcceptanceManager* manager);
Acceptance::FirmwareAcceptanceManager* currentFirmwareAcceptanceManager(); // Get current firmware acceptance manager instance

// Rollback manager registration (Phase 6D)
void registerRollbackManager(Rollback::RollbackManager* manager);
Rollback::RollbackManager* rollbackManager(); // Get current rollback manager instance

VersionComparison compareVersions(uint32_t currentVersionCode, uint32_t remoteVersionCode);

// Phase 3 update methods - updated public API
UpdateDecision checkForUpdate();
const UpdateInfo& getLatestUpdateInfo(); // Returns const reference to internal static data
void setLatestUpdateInfo(const UpdateInfo& info);
UpdateDecisionContext getUpdateDecisionContext(); // Returns copy of internal context

// Diagnostic method (only active when OTA_PLATFORM_VALIDATION is enabled)
void printUpdateStatus();
}
