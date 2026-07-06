#include "ota_installer.h"
#include <esp_random.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
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
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] shutdown() called but already shutdown - idempotent operation\n");
        #endif
        return;
    }

    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Beginning shutdown sequence...\n");
    if (_activeSession.isActive) {
        printf("[OtaInstaller] Active session detected, aborting before shutdown\n");
    }
    #endif

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

    // Initialize progress tracking
    _currentProgress.bytesWritten = 0;
    _currentProgress.totalBytes = context.payloadDescriptor.payloadSize;
    _currentProgress.startTimeMs = millis();
    _currentProgress.lastUpdateMs = _currentProgress.startTimeMs;
    _currentProgress.currentStage = InstallationStage::ContextValidation;

    transitionTo(InstallationState::Preparing);
    
    // Phase 6B.1 audit: Validate payload size before attempting any OTA operations
    const uint32_t payload_size = context.payloadDescriptor.payloadSize;
    if (payload_size == 0) {
        setError(InstallationError::InvalidContext, "Zero-byte firmware payload rejected");
        transitionTo(InstallationState::Error);
        _activeSession.isActive = false;
        return false;
    }
    
    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Validating payload size: %lu bytes\n", static_cast<unsigned long>(payload_size));
    #endif

    // Phase 6B.1 audit: Get both running and target partitions for safety verification
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(nullptr);
    
    #ifdef OTA_PLATFORM_VALIDATION
    if (running_partition != nullptr) {
        printf("[OtaInstaller] Running partition: %s at address 0x%08lx\n", 
               running_partition->label, running_partition->address);
    }
    #endif
    
    if (update_partition == nullptr) {
        setError(InstallationError::PartitionUnavailable, "Failed to discover OTA partition");
        transitionTo(InstallationState::Error);
        _activeSession.isActive = false;
        return false;
    }
    
    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Target OTA partition: %s at address 0x%08lx, size: %ld bytes\n", 
           update_partition->label, update_partition->address, update_partition->size);
    #endif

    // Phase 6B.1 audit: CRITICAL SAFETY CHECK - running partition must not equal target partition
    if (running_partition != nullptr && update_partition != nullptr && 
        running_partition->address == update_partition->address) {
        setError(InstallationError::PartitionUnavailable, "Running partition cannot be used for OTA update");
        transitionTo(InstallationState::Error);
        _activeSession.isActive = false;
        return false;
    }
    
    // Phase 6B.1 audit: Verify payload fits in the target partition
    if (payload_size > update_partition->size) {
        setError(InstallationError::InvalidContext, "Firmware payload exceeds partition capacity");
        transitionTo(InstallationState::Error);
        _activeSession.isActive = false;
        return false;
    }

    // Store partition and transition state to PartitionSelected - cast to opaque uintptr_t for storage
    _activeSession.otaHandle._partition = reinterpret_cast<uintptr_t>(update_partition);
    _activeSession.otaHandle._sessionState = OtaSessionState::PartitionSelected;
    
    // Phase 6B.1 audit: Create real OTA session with esp_ota_begin() - payload size is fully validated
    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, payload_size, &ota_handle);
    
    if (err != ESP_OK) {
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] esp_ota_begin() failed with error: %d\n", err);
        #endif
        setError(InstallationError::InternalError, "esp_ota_begin() failed");
        transitionTo(InstallationState::Error);
        cleanupOtaSession(); // Properly clean up any partial state
        _activeSession.isActive = false;
        return false;
    }
    
    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] OTA session created successfully, handle: %u, image size: %lu bytes\n", 
           ota_handle, static_cast<unsigned long>(payload_size));
    #endif

    // Store OTA handle and mark session as open - this transitions our internal state machine
    _activeSession.otaHandle._otaHandle = static_cast<uint32_t>(ota_handle);
    _activeSession.otaHandle._sessionState = OtaSessionState::SessionOpen;

    // Preparation complete - transition to Installing
    _currentProgress.currentStage = InstallationStage::DataTransfer;
    transitionTo(InstallationState::Installing);

    #ifdef OTA_PLATFORM_VALIDATION
    // Validate invariants after beginning installation
    validateProgressInvariants();
    #endif

    return true;
}

#ifdef OTA_PLATFORM_VALIDATION
void OtaInstaller::validateProgressInvariants() {
    const uint32_t bytesWritten = _currentProgress.bytesWritten;
    const uint32_t totalBytes = _currentProgress.totalBytes;
    const uint32_t remaining = totalBytes - bytesWritten;
    const uint8_t percentage = _currentProgress.percentage();
    const uint32_t writeCount = _activeSession.chunksWritten;
    const uint32_t lastChunkSize = _activeSession.lastChunkSize;

    // Invariant 1: bytesWritten <= totalBytes
    if (bytesWritten > totalBytes) {
        printf("[OTA][INVARIANT] bytesWritten exceeds totalBytes\n");
        printf("[OTA][INVARIANT] bytesWritten=%u, totalBytes=%u\n", bytesWritten, totalBytes);
    }

    // Invariant 2: remainingBytes == totalBytes - bytesWritten (always true by calculation, but log if mismatch)
    const uint32_t expectedRemaining = totalBytes - bytesWritten;
    if (remaining != expectedRemaining) {
        printf("[OTA][INVARIANT] remainingBytes mismatch\n");
        printf("[OTA][INVARIANT] bytesWritten=%u, remaining=%u, expectedRemaining=%u, totalBytes=%u\n",
               bytesWritten, remaining, expectedRemaining, totalBytes);
    }

    // Invariant 3: percentage <= 100
    if (percentage > 100) {
        printf("[OTA][INVARIANT] progress percentage overflow\n");
        printf("[OTA][INVARIANT] percentage=%u, bytesWritten=%u, totalBytes=%u\n",
               percentage, bytesWritten, totalBytes);
    }

    // Invariant 4: writeCount > 0 || bytesWritten == 0
    if (bytesWritten > 0 && writeCount == 0) {
        printf("[OTA][INVARIANT] write accounting inconsistent\n");
        printf("[OTA][INVARIANT] bytesWritten=%u, writeCount=%u\n", bytesWritten, writeCount);
    }

    // Invariant 5: if bytesWritten == totalBytes, then remaining must be 0
    if (bytesWritten == totalBytes && remaining != 0) {
        printf("[OTA][INVARIANT] completed transfer has remaining bytes\n");
        printf("[OTA][INVARIANT] bytesWritten=%u, totalBytes=%u, remaining=%u\n",
               bytesWritten, totalBytes, remaining);
    }

    // Invariant 6: if remaining == totalBytes, then bytesWritten must be 0
    if (remaining == totalBytes && bytesWritten != 0) {
        printf("[OTA][INVARIANT] untouched transfer accounting invalid\n");
        printf("[OTA][INVARIANT] remaining=%u, totalBytes=%u, bytesWritten=%u\n",
               remaining, totalBytes, bytesWritten);
    }

    // Invariant 7: percentage == 100 only when bytesWritten == totalBytes
    if (percentage == 100 && bytesWritten < totalBytes) {
        printf("[OTA][INVARIANT] 100%% reported before transfer complete\n");
        printf("[OTA][INVARIANT] percentage=100, bytesWritten=%u, totalBytes=%u\n",
               bytesWritten, totalBytes);
    }

    // Invariant 8: percentage == 0 when bytesWritten == 0
    if (bytesWritten == 0 && percentage != 0) {
        printf("[OTA][INVARIANT] non-zero percentage with zero bytes written\n");
        printf("[OTA][INVARIANT] percentage=%u, bytesWritten=0, totalBytes=%u\n",
               percentage, totalBytes);
    }

    // Optional Strong Invariant: bytesWritten + remaining == totalBytes
    if ((bytesWritten + remaining) != totalBytes) {
        printf("[OTA][INVARIANT] total bytes accounting failure\n");
        printf("[OTA][INVARIANT] bytesWritten=%u, remaining=%u, sum=%u, totalBytes=%u\n",
               bytesWritten, remaining, bytesWritten + remaining, totalBytes);
    }

    // Optional: lastChunkSize <= totalBytes
    if (lastChunkSize > totalBytes && totalBytes > 0) {
        printf("[OTA][INVARIANT] lastChunkSize exceeds total firmware size\n");
        printf("[OTA][INVARIANT] lastChunkSize=%u, totalBytes=%u\n", lastChunkSize, totalBytes);
    }

    // Optional: lastChunkSize > 0 only if writeCount > 0
    if (writeCount == 0 && lastChunkSize != 0) {
        printf("[OTA][INVARIANT] lastChunkSize set with zero write count\n");
        printf("[OTA][INVARIANT] lastChunkSize=%u, writeCount=%u\n", lastChunkSize, writeCount);
    }
}
#endif

bool OtaInstaller::write(const void* data, size_t length) {
    // STRICT: write() is ONLY legal in Installing state - enforce this
    if (_currentState != InstallationState::Installing) {
        setError(InstallationError::InvalidState, "write() can only be called in Installing state");
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] write() rejected - invalid state: %d\n", static_cast<int>(_currentState));
        #endif
        return false;
    }

    if (!_activeSession.isActive) {
        setError(InstallationError::InvalidState, "No active installation session");
        return false;
    }

    // Reject invalid write parameters immediately
    if (data == nullptr) {
        setError(InstallationError::InvalidContext, "Null buffer write rejected");
        return false;
    }
    if (length == 0) {
        setError(InstallationError::InvalidContext, "Zero-byte write rejected");
        return false;
    }

    // Verify we don't write more than remaining firmware size
    const uint32_t remaining = _currentProgress.totalBytes - _currentProgress.bytesWritten;
    if (length > remaining) {
        setError(InstallationError::InvalidContext, "Write exceeds remaining firmware size");
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] Oversized write rejected: %zu bytes, only %u remaining\n", length, remaining);
        #endif
        return false;
    }

    // Verify OTA session is actually open and can be written to
    if (!_activeSession.otaHandle.isOpen()) {
        setError(InstallationError::InvalidState, "OTA session not open for writing");
        return false;
    }

    // Extract real ESP-IDF handle from opaque storage
    esp_ota_handle_t ota_handle = static_cast<esp_ota_handle_t>(_activeSession.otaHandle._otaHandle);
    
    // Perform actual flash write with esp_ota_write()
    esp_err_t err = esp_ota_write(ota_handle, data, length);
    
    if (err != ESP_OK) {
        // On write failure, immediately stop and transition to error
        setError(InstallationError::WriteFailed, "esp_ota_write() failed");
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] esp_ota_write() failed: %s (chunk size: %zu bytes)\n", esp_err_to_name(err), length);
        #endif
        transitionTo(InstallationState::Error);
        cleanupOtaSession(); // Clean up invalid session
        _activeSession.isActive = false;
        return false;
    }

    // Write succeeded - update all progress counters ONLY after successful flash write
    _activeSession.chunksReceived++;
    _activeSession.chunksWritten++;
    _activeSession.lastChunkSize = static_cast<uint32_t>(length);
    _activeSession.lastWriteTimestampMs = millis();
    
    // Update progress with exact bytes written - never report failed writes
      _currentProgress.bytesWritten += static_cast<uint32_t>(length);
      _currentProgress.lastUpdateMs = millis();

      // Calculate local values for diagnostics (no need to store in progress struct)
      const uint32_t remaining_after = _currentProgress.totalBytes - _currentProgress.bytesWritten;
      const uint8_t current_percentage = _currentProgress.percentage();

     #ifdef OTA_PLATFORM_VALIDATION
      printf("[OtaInstaller] write() succeeded: chunk=%zu bytes, written=%u/%u (%u%%), remaining=%u, write_count=%u\n",
             length, _currentProgress.bytesWritten, _currentProgress.totalBytes,
             current_percentage, remaining_after, _activeSession.chunksWritten);
      // Validate invariants after every successful write
      validateProgressInvariants();
      #endif

    return true;
}

InstallationResult OtaInstaller::finalizeInstallation() {
    InstallationResult result;

    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Entering finalizeInstallation()\n");
    printf("[OtaInstaller] Installer state: %s\n", stateToString(_currentState));
    printf("[OtaInstaller] OTA session state: %d\n", static_cast<int>(_activeSession.otaHandle._sessionState));
    printf("[OtaInstaller] bytesWritten=%u, totalBytes=%u, remaining=%u, percentage=%u, writeCount=%u\n",
           _currentProgress.bytesWritten, _currentProgress.totalBytes,
           (_currentProgress.totalBytes - _currentProgress.bytesWritten),
           _currentProgress.percentage(), _activeSession.chunksWritten);
    #endif

    // STRICT: finalizeInstallation() is ONLY legal in Installing state - enforce this
    if (_currentState != InstallationState::Installing) {
        result.status = InstallationStatus::Failed;
        result.error = InstallationError::InvalidState;
        result.errorMessage = "finalizeInstallation() can only be called in Installing state";
        setError(result.error, result.errorMessage);
        
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] Rejecting finalize: invalid installer state\n");
        #endif
        return result;
    }

    if (!_activeSession.isActive) {
        result.status = InstallationStatus::Failed;
        result.error = InstallationError::InvalidState;
        result.errorMessage = "No active session to finalize";
        setError(result.error, result.errorMessage);
        
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] Rejecting finalize: no active session\n");
        #endif
        return result;
    }

    // Comprehensive pre-finalization validation - ALL conditions must pass
    const uint32_t remainingBytes = _currentProgress.totalBytes - _currentProgress.bytesWritten;
    const bool preflight_ok = 
        (_currentState == InstallationState::Installing) &&
        (_activeSession.otaHandle._sessionState == OtaSessionState::SessionOpen) &&
        (_currentProgress.bytesWritten == _currentProgress.totalBytes) &&
        (remainingBytes == 0) &&
        (_currentProgress.percentage() == 100) &&
        (_activeSession.chunksWritten > 0);

    if (!preflight_ok) {
        const char* err = "Pre-finalization validation failed - invariants violated";
        setError(InstallationError::FinalizeFailed, err);
        result.status = InstallationStatus::Failed;
        result.error = _lastError;
        result.errorMessage = err;
        transitionTo(InstallationState::Error);
        
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] Pre-flight validation FAILED - rejecting esp_ota_end()\n");
        printf("[OtaInstaller] InstallerState==Installing: %d, SessionOpen: %d\n", 
               (_currentState == InstallationState::Installing),
               (_activeSession.otaHandle._sessionState == OtaSessionState::SessionOpen));
        printf("[OtaInstaller] bytesWritten==totalBytes: %d, remaining==0: %d, percentage==100: %d, writeCount>0: %d\n",
               (_currentProgress.bytesWritten == _currentProgress.totalBytes),
               (remainingBytes == 0),
               (_currentProgress.percentage() == 100),
               (_activeSession.chunksWritten > 0));
        #endif
        
        // Failed preflight - cleanup will call esp_ota_abort() since session is still open
        cleanupOtaSession();
        _activeSession.isActive = false;
        _activeSession.otaHandle._otaHandle = 0;
        _lastResult = result;
        return result;
    }

    // Extract real ESP-IDF handle from opaque storage - remains valid until esp_ota_end() completes
    esp_ota_handle_t ota_handle = static_cast<esp_ota_handle_t>(_activeSession.otaHandle._otaHandle);

    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] All pre-flight checks PASSED - calling esp_ota_end() with handle=0x%x\n", 
           static_cast<unsigned int>(ota_handle));
    #endif

    transitionTo(InstallationState::Finalizing);
    _currentProgress.currentStage = InstallationStage::WriteVerification;

    // Phase 6B.3: Call esp_ota_end() to finalize the OTA transaction
    // HANDLE REMAINS VALID during this call - never invalidated before esp_ota_end() completes
    esp_err_t err = esp_ota_end(ota_handle);

    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] esp_ota_end() returned: %s (0x%x)\n", esp_err_to_name(err), err);
    #endif

    if (err != ESP_OK) {
        // esp_ota_end() failed - transition to Error state, session remains open for cleanup
        char error_buf[128];
        snprintf(error_buf, sizeof(error_buf), "esp_ota_end() failed: %s", esp_err_to_name(err));
        setError(InstallationError::FinalizeFailed, error_buf);
        result.status = InstallationStatus::Failed;
        result.error = _lastError;
        result.errorMessage = errorToString(_lastError);
        transitionTo(InstallationState::Error);
        
        // cleanupOtaSession() will see SessionOpen and call esp_ota_abort() correctly
        cleanupOtaSession();
        _activeSession.isActive = false;
        _activeSession.otaHandle._otaHandle = 0; // Now safe to invalidate
        _activeSession.otaHandle._sessionState = OtaSessionState::Invalid;
        
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] Finalization FAILED - entering cleanup path\n");
        printf("[OtaInstaller] ESP-IDF error: %s (0x%x)\n", esp_err_to_name(err), err);
        printf("[OtaInstaller] Session state transitioned to: Invalid\n");
        printf("[OtaInstaller] Final installer state: %s\n", stateToString(_currentState));
        #endif
        
        validateProgressInvariants();
        _lastResult = result;
        return result;
    }

    // SUCCESS: esp_ota_end() completed successfully - ONLY NOW mark as SessionClosed
    _activeSession.otaHandle._sessionState = OtaSessionState::SessionClosed;
    
    // Now safe to invalidate the handle - esp_ota_end() has completed successfully
    _activeSession.otaHandle._otaHandle = 0;
    
    _currentProgress.currentStage = InstallationStage::Complete;
    transitionTo(InstallationState::Completed);
    
    // Progress data is PRESERVED - do NOT overwrite bytesWritten, totalBytes, etc.
    // Only mark session as inactive, diagnostics remain available
    _activeSession.isActive = false;
    
    // cleanupOtaSession() will see SessionClosed and SKIP esp_ota_abort() - this is CRITICAL
    cleanupOtaSession();

    // Populate successful result - NO boot partition changes (handled in Phase 6B.4)
    result.status = InstallationStatus::Success;
    result.error = InstallationError::None;
    result.finalProgress = _currentProgress;
    result.session = _activeSession;
    result.targetPartitionId = _activeContext.targetPartitionId;
    result.bootPartitionUpdated = false;

    _lastResult = result;
    
    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Installation completed successfully\n");
    printf("[OtaInstaller] Session closed, installer state=Completed, handle invalidated\n");
    #endif

    validateProgressInvariants();
    return result;
}

void OtaInstaller::abort() {
    // STRICT: abort() is ONLY legal during active installation: Preparing, Installing, Finalizing
    if (!(_currentState == InstallationState::Preparing || 
          _currentState == InstallationState::Installing || 
          _currentState == InstallationState::Finalizing)) {
        setError(InstallationError::InvalidState, "abort() can only be called during active installation");
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] abort() called from invalid state: %d\n", static_cast<int>(_currentState));
        #endif
        return;
    }

    transitionTo(InstallationState::Aborted);
    
    #ifdef OTA_PLATFORM_VALIDATION
    if (_activeSession.otaHandle.isOpen()) {
        printf("[OtaInstaller] Aborting active OTA session, handle: %u\n", 
               static_cast<unsigned int>(_activeSession.otaHandle._otaHandle));
    } else {
        printf("[OtaInstaller] Aborting installation (no active OTA session to clean up)\n");
    }
    #endif
    
    // Clean up any active OTA session - canAbort() check inside ensures we never call esp_ota_abort() on invalid session
    cleanupOtaSession();
    
    _activeSession.isActive = false;
    _lastError = InstallationError::Cancelled;
    
    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Installation aborted successfully\n");
    // Validate invariants after cleanup
    validateProgressInvariants();
    #endif
}

void OtaInstaller::reset() {
    // reset() is the ONLY way to transition out of: Completed, Aborted, Error states
    // This enforces that Completed requires explicit reset() before starting a new installation
    
    #ifdef OTA_PLATFORM_VALIDATION
    printf("[OtaInstaller] Beginning reset sequence from state: %d\n", static_cast<int>(_currentState));
    if (_activeSession.otaHandle.isOpen()) {
        printf("[OtaInstaller] Open OTA session detected during reset, will be cleaned up\n");
    }
    #endif
    
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
        // Validate invariants after reset
        validateProgressInvariants();
        #endif
    } else {
        _currentState = InstallationState::Idle;
        _currentProgress.currentState = _currentState;
        #ifdef OTA_PLATFORM_VALIDATION
        printf("[OtaInstaller] Reset complete - transitioned to Idle\n");
        // Validate invariants after reset
        validateProgressInvariants();
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

void OtaInstaller::cleanupOtaSession() {
    // Only attempt to abort if session is actually open
    if (_activeSession.otaHandle.canAbort()) {
        // If we have an active OTA session, we must clean it up with esp_ota_abort()
        // This is required because ESP-IDF tracks open OTA sessions and they must be closed
        esp_ota_handle_t handle = static_cast<esp_ota_handle_t>(_activeSession.otaHandle._otaHandle);
        esp_err_t err = esp_ota_abort(handle);
        
        #ifdef OTA_PLATFORM_VALIDATION
        if (err == ESP_OK) {
            printf("[OtaInstaller] OTA session aborted successfully, handle: %u\n", handle);
        } else {
            printf("[OtaInstaller] Warning: esp_ota_abort() failed with error: %d\n", err);
        }
        #endif
    }
    
    // Always reset all OTA session state to Invalid, regardless of cleanup result
    // This ensures no stale state remains and prevents double-abort issues
    _activeSession.otaHandle._otaHandle = 0;
    _activeSession.otaHandle._partition = 0;
    _activeSession.otaHandle._sessionState = OtaSessionState::Invalid;
}

void OtaInstaller::clearInternalState() {
    _lastError = InstallationError::None;
    
    // Clean up any active OTA session first
    cleanupOtaSession();
    
    // Clear active session
    if (_activeSession.isActive) {
        _activeSession.isActive = false;
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
    
    #ifdef OTA_PLATFORM_VALIDATION
    // Validate invariants after internal state clear
    validateProgressInvariants();
    #endif
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