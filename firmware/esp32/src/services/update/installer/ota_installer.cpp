#include "ota_installer.h"
#include <esp_random.h>
#include <cstring>

namespace Installation {

OtaInstaller::OtaInstaller()
    : _currentState(InstallationState::Idle)
    , _lastError(InstallationError::None)
    , _isInitialized(false) {
    clearInternalState();
}

OtaInstaller::~OtaInstaller() {
    if (_isInitialized) {
        shutdown();
    }
}

bool OtaInstaller::initialize() {
    // Idempotent: allow multiple initialize() calls, no-op if already initialized
    if (_isInitialized && _currentState == InstallationState::Ready) {
        return true;
    }

    if (!canTransitionTo(InstallationState::Initializing)) {
        setError(InstallationError::InvalidState, "Cannot initialize from current state");
        return false;
    }

    transitionTo(InstallationState::Initializing);
    
    // Passive initialization - no resources allocated in Phase 6A
    _isInitialized = true;
    
    transitionTo(InstallationState::Ready);
    return true;
}

void OtaInstaller::shutdown() {
    // Idempotent: allow multiple shutdown() calls, no-op if already shutdown
    if (!_isInitialized && _currentState == InstallationState::Idle) {
        return;
    }

    if (_activeSession.isActive) {
        abort();
    }
    
    clearInternalState();
    _isInitialized = false;
    // Can't use transitionTo() for this special case, set directly like reset() does
    _currentState = InstallationState::Idle;
    _currentProgress.currentState = _currentState;
    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Shutdown complete - transitioned to Idle\n");
    #endif
}

bool OtaInstaller::beginInstallation(const InstallationContext& context) {
    if (!_isInitialized) {
        setError(InstallationError::NotInitialized, "Installer not initialized");
        return false;
    }

    if (_activeSession.isActive || isBusy()) {
        setError(InstallationError::Busy, "Installation already in progress");
        return false;
    }

    if (!canTransitionTo(InstallationState::Preparing)) {
        setError(InstallationError::InvalidState, "Cannot begin installation from current state");
        return false;
    }

    if (!context.isValid()) {
        setError(InstallationError::InvalidContext, "Installation context is invalid");
        return false;
    }

    // Store the context
    _activeContext = context;
    
    // Create new session
    _activeSession.sessionId = generateSessionId();
    _activeSession.creationTimestampMs = millis();
    _activeSession.isActive = true;
    _activeSession.chunksReceived = 0;
    _activeSession.chunksWritten = 0;
    _activeSession.otaHandle._isValid = true; // Mark handle as active for this session

    // Initialize progress tracking
    _currentProgress.bytesWritten = 0;
    _currentProgress.totalBytes = context.payloadDescriptor.payloadSize;
    _currentProgress.startTimeMs = millis();
    _currentProgress.lastUpdateMs = _currentProgress.startTimeMs;
    _currentProgress.currentStage = InstallationStage::ContextValidation;

    transitionTo(InstallationState::Preparing);
    
    // Simulate preparation completion
    _currentProgress.currentStage = InstallationStage::DataTransfer;
    transitionTo(InstallationState::Installing);

    return true;
}

bool OtaInstaller::write(const void* data, size_t length) {
    // STRICT: write() is ONLY legal in Installing state - enforce this
    if (_currentState != InstallationState::Installing) {
        setError(InstallationError::InvalidState, "write() can only be called in Installing state");
        return false;
    }

    if (!_activeSession.isActive) {
        setError(InstallationError::InvalidState, "No active installation session");
        return false;
    }

    if (data == nullptr || length == 0) {
        setError(InstallationError::InvalidContext, "Invalid write parameters");
        return false;
    }

    // Passive write - no actual flash writing in Phase 6A
    _activeSession.chunksReceived++;
    _activeSession.chunksWritten++;
    _activeSession.lastChunkSize = static_cast<uint32_t>(length);
    _activeSession.lastWriteTimestampMs = millis();
    
    _currentProgress.bytesWritten += static_cast<uint32_t>(length);
    _currentProgress.lastUpdateMs = millis();

    return true;
}

InstallationResult OtaInstaller::finalizeInstallation() {
    InstallationResult result;

    // STRICT: finalizeInstallation() is ONLY legal in Installing state - enforce this
    if (_currentState != InstallationState::Installing) {
        result.status = InstallationStatus::Failed;
        result.error = InstallationError::InvalidState;
        result.errorMessage = "finalizeInstallation() can only be called in Installing state";
        setError(result.error, result.errorMessage);
        return result;
    }

    if (!_activeSession.isActive) {
        result.status = InstallationStatus::Failed;
        result.error = InstallationError::InvalidState;
        result.errorMessage = "No active session to finalize";
        setError(result.error, result.errorMessage);
        return result;
    }

    transitionTo(InstallationState::Finalizing);
    _currentProgress.currentStage = InstallationStage::WriteVerification;

    // Verify all bytes were written
    if (_currentProgress.bytesWritten != _currentProgress.totalBytes) {
        setError(InstallationError::FinalizeFailed, "Incomplete firmware write");
        result.status = InstallationStatus::Failed;
        result.error = _lastError;
        result.errorMessage = errorToString(_lastError);
        transitionTo(InstallationState::Error);
        _activeSession.isActive = false;
        _activeSession.otaHandle._isValid = false; // Invalidate ESP-IDF handle
        _lastResult = result;
        return result;
    }

    // Passive finalization - no ESP-IDF calls in Phase 6A
    _currentProgress.currentStage = InstallationStage::Complete;
    transitionTo(InstallationState::Completed);

    // Populate successful result
    result.status = InstallationStatus::Success;
    result.error = InstallationError::None;
    result.finalProgress = _currentProgress;
    result.session = _activeSession;
    result.targetPartitionId = _activeContext.targetPartitionId;
    result.bootPartitionUpdated = false; // Placeholder - not implemented in 6A

    _activeSession.isActive = false;
    _activeSession.otaHandle._isValid = false; // Invalidate ESP-IDF handle
    _lastResult = result;

    return result;
}

void OtaInstaller::abort() {
    // STRICT: abort() is ONLY legal during active installation: Preparing, Installing, Finalizing
    if (!(_currentState == InstallationState::Preparing || 
          _currentState == InstallationState::Installing || 
          _currentState == InstallationState::Finalizing)) {
        setError(InstallationError::InvalidState, "abort() can only be called during active installation");
        return;
    }

    transitionTo(InstallationState::Aborted);
    _activeSession.isActive = false;
    _activeSession.otaHandle._isValid = false; // Invalidate ESP-IDF handle
    _lastError = InstallationError::Cancelled;
}

void OtaInstaller::reset() {
    // reset() is the ONLY way to transition out of: Completed, Aborted, Error states
    // This enforces that Completed requires explicit reset() before starting a new installation
    
    if (_activeSession.isActive) {
        // If an installation is still in progress, abort it first
        if (_currentState == InstallationState::Preparing || 
            _currentState == InstallationState::Installing || 
            _currentState == InstallationState::Finalizing) {
            abort();
        }
    }
    
    clearInternalState();
    
    // Only reset() can perform these special transitions not in the transition matrix
    if (_isInitialized) {
        _currentState = InstallationState::Ready;
        _currentProgress.currentState = _currentState;
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] Reset complete - transitioned to Ready\n");
        #endif
    } else {
        _currentState = InstallationState::Idle;
        _currentProgress.currentState = _currentState;
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] Reset complete - transitioned to Idle\n");
        #endif
    }
}

InstallationState OtaInstaller::state() const {
    return _currentState;
}

const InstallationProgress& OtaInstaller::progress() const {
    return _currentProgress;
}

const InstallationCapabilities& OtaInstaller::capabilities() const {
    return _capabilities;
}

InstallationError OtaInstaller::lastError() const {
    return _lastError;
}

const InstallationResult& OtaInstaller::lastResult() const {
    return _lastResult;
}

bool OtaInstaller::isBusy() const {
    return _currentState == InstallationState::Preparing || 
           _currentState == InstallationState::Installing || 
           _currentState == InstallationState::Finalizing;
}

bool OtaInstaller::isReady() const {
    return _isInitialized && _currentState == InstallationState::Ready;
}

bool OtaInstaller::canTransitionTo(InstallationState newState) const {
    const uint8_t currentIdx = stateToIndex(_currentState);
    const auto& validNextStates = _validTransitions[currentIdx];
    
    for (const auto& validState : validNextStates) {
        if (validState == newState) {
            return true;
        }
        // Stop at default-initialized state (end of valid transitions list)
        if (validState == InstallationState::Idle && newState != InstallationState::Idle) {
            break;
        }
    }
    
    // Special case: reset() can transition ANY state back to Ready or Idle
    if (newState == InstallationState::Ready || newState == InstallationState::Idle) {
        return false; // Only reset() can do this, which calls clearInternalState first
    }
    
    return false;
}

void OtaInstaller::transitionTo(InstallationState newState) {
    if (_currentState == newState) {
        return;
    }

    #ifdef OTA_PLATFORM_VALIDATION
    // Only log transitions in diagnostic mode, never in production
    printf("[OtaInstaller] State transition: %s -> %s\n", 
           stateToString(_currentState), stateToString(newState));
    #endif

    _currentState = newState;
    _currentProgress.currentState = newState;
}

void OtaInstaller::clearInternalState() {
    _lastError = InstallationError::None;
    
    // Clear active session
    if (_activeSession.isActive) {
        _activeSession.isActive = false;
        _activeSession.otaHandle._isValid = false; // Invalidate ESP-IDF handle placeholder
    }
    _activeSession.sessionId = 0;
    _activeSession.creationTimestampMs = 0;
    _activeSession.chunksReceived = 0;
    _activeSession.chunksWritten = 0;
    _activeSession.lastChunkSize = 0;
    _activeSession.lastWriteTimestampMs = 0;
    
    // Reset progress tracking
    memset(&_currentProgress, 0, sizeof(_currentProgress));
    _currentProgress.currentState = InstallationState::Idle;
    _currentProgress.currentStage = InstallationStage::NotStarted;
    
    // Reset last result
    memset(&_lastResult, 0, sizeof(_lastResult));
    _lastResult.status = InstallationStatus::NotStarted;
    _lastResult.error = InstallationError::None;
}

void OtaInstaller::setError(InstallationError error, const char* message) {
    _lastError = error;
    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Error: %s (%s)\n", errorToString(error), message);
    #endif
}

uint64_t OtaInstaller::generateSessionId() {
    // Generate random session ID using ESP32 HW RNG
    uint64_t id = 0;
    id = static_cast<uint64_t>(esp_random()) << 32;
    id |= esp_random();
    return id;
}

} // namespace Installation