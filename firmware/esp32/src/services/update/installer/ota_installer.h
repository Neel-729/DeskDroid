#pragma once

#include <Arduino.h>
#include "iinstaller.h"

namespace Installation {

/**
 * @brief OTA-specific installer implementation for ESP32
 * 
 * Passive implementation that manages the state machine and installation lifecycle
 * without calling any ESP-IDF OTA APIs or writing to flash. All operations are
 * simulated to maintain the architecture while Phase 6B connects to real hardware.
 */
class OtaInstaller : public IInstaller {
public:
    OtaInstaller();
    ~OtaInstaller() override;

    // IInstaller interface implementation
    bool initialize() override;
    void shutdown() override;
    bool beginInstallation(const InstallationContext& context) override;
    bool write(const void* data, size_t length) override;
    InstallationResult finalizeInstallation() override;
    ActivationResult activateInstalledFirmware() override;
    void abort() override;
    void reset() override;
    InstallationState state() const override;
    const InstallationProgress& progress() const override;
    const InstallationCapabilities& capabilities() const override;
    InstallationError lastError() const override;
    const InstallationResult& lastResult() const override;
    bool isBusy() const override;
    bool isReady() const override;

private:
    // State machine validation
    bool canTransitionTo(InstallationState newState) const;
    void transitionTo(InstallationState newState);
    
    // Internal state reset helpers
    void cleanupOtaSession();
    void clearInternalState();
    void setError(InstallationError error, const char* message);
    
    // Session management
    uint64_t generateSessionId();
    
    // Debug-only invariant validation
    #ifdef OTA_PLATFORM_VALIDATION
    void validateProgressInvariants();
    #endif

    // State tracking
    InstallationState _currentState;
    InstallationProgress _currentProgress;
    InstallationCapabilities _capabilities;
    InstallationError _lastError;
    InstallationResult _lastResult;
    InstallationContext _activeContext;
    InstallationSession _activeSession;
    
    bool _isInitialized;
    bool _activationCompleted;

    // State transition matrix - defines ALL valid state transitions
    // Strict matrix - Completed state can ONLY transition out via explicit reset()
    static constexpr std::array<std::array<InstallationState, 8>, 9> _validTransitions = {{
        // From Idle (0) - only can initialize
        {InstallationState::Initializing, InstallationState::Error},
        // From Initializing (1) - must complete or fail
        {InstallationState::Ready, InstallationState::Error},
        // From Ready (2) - only start installation or error
        {InstallationState::Preparing, InstallationState::Error},
        // From Preparing (3) - start installing, abort, or error
        {InstallationState::Installing, InstallationState::Aborted, InstallationState::Error},
        // From Installing (4) - finalize, abort, or error
        {InstallationState::Finalizing, InstallationState::Aborted, InstallationState::Error},
        // From Finalizing (5) - only complete or error
        {InstallationState::Completed, InstallationState::Error},
        // From Completed (6) - CANNOT transition automatically - requires explicit reset()
        {InstallationState::Error},
        // From Aborted (7) - can only reset to Ready or error
        {InstallationState::Ready, InstallationState::Error},
        // From Error (8) - can only reset to Ready or Idle
        {InstallationState::Ready, InstallationState::Idle}
    }};

    // Helper to get index for state in transition matrix
    static constexpr uint8_t stateToIndex(InstallationState state) {
        return static_cast<uint8_t>(state);
    }
};

} // namespace Installation
