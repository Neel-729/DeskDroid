#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <array>
#include "../verification/verification_types.h"

namespace Installation {

/**
 * @brief Installation state - strongly typed state machine states
 */
enum class InstallationState : uint8_t {
    Idle,           ///< Installer is idle, ready to begin
    Initializing,   ///< Installer is initializing resources
    Ready,          ///< Installer is initialized and ready
    Preparing,      ///< Preparing for installation (partition selection etc.)
    Installing,     ///< Actively writing firmware data
    Finalizing,     ///< Finalizing installation (verifying write)
    Completed,      ///< Installation completed successfully
    Aborted,        ///< Installation was manually aborted
    Error           ///< Installation failed with error
};

/**
 * @brief Installation status - high-level outcome
 */
enum class InstallationStatus : uint8_t {
    Success,        ///< Installation completed successfully
    Failed,         ///< Installation failed
    Cancelled,      ///< Installation was cancelled
    InProgress,     ///< Installation still in progress
    NotStarted      ///< Installation has not been started
};

/**
 * @brief Installation error codes - dedicated error domain
 */
enum class InstallationError : uint8_t {
    None,                   ///< No error
    NotInitialized,         ///< Installer not initialized
    Busy,                   ///< Installer is busy with another installation
    InvalidContext,         ///< Installation context is invalid or incomplete
    InvalidState,           ///< Operation not valid in current state
    Cancelled,              ///< Operation was cancelled
    WriteFailed,            ///< Failed to write data chunk
    PartitionUnavailable,   ///< Target partition is not available
    FinalizeFailed,         ///< Failed to finalize installation
    InternalError,          ///< Internal installer error
    Unsupported            ///< Operation not supported by this installer
};

/**
 * @brief Installation stage - tracks progress through installation pipeline
 */
enum class InstallationStage : uint8_t {
    NotStarted,
    ContextValidation,
    PartitionPreparation,
    DataTransfer,
    WriteVerification,
    BootPartitionUpdate,
    Complete
};

/**
 * @brief Installation capabilities - what this installer supports
 */
struct InstallationCapabilities {
    bool supportsStreaming : 1;           ///< Supports streaming chunked writes
    bool supportsResume : 1;              ///< Supports resuming interrupted installations
    bool supportsRollbackPreparation : 1; ///< Can prepare for rollback scenarios
    bool supportsPartitionSelection : 1;  ///< Can select between multiple partitions
    bool supportsProgressReporting : 1;   ///< Provides detailed progress reporting

    constexpr InstallationCapabilities()
        : supportsStreaming(true)
        , supportsResume(false)
        , supportsRollbackPreparation(false)
        , supportsPartitionSelection(true)
        , supportsProgressReporting(true) {}
};

/**
 * @brief Installation policy - configuration for installation behavior
 */
struct InstallationPolicy {
    bool verifyAfterWrite : 1;        ///< Verify written data before completion
    bool allowDowngrades : 1;         ///< Allow installing older firmware versions
    bool forceSinglePartition : 1;    ///< Force use of specific partition
    bool resetOnFailure : 1;          ///< Automatically reset state on failure

    constexpr InstallationPolicy()
        : verifyAfterWrite(true)
        , allowDowngrades(false)
        , forceSinglePartition(false)
        , resetOnFailure(true) {}
};

/**
 * @brief Installation progress - tracking for ongoing installations
 */
struct InstallationProgress {
    uint32_t bytesWritten;            ///< Total bytes successfully written
    uint32_t totalBytes;              ///< Total bytes expected for this installation
    uint64_t startTimeMs;             ///< When installation started (millis())
    uint64_t lastUpdateMs;            ///< Last progress update timestamp
    InstallationStage currentStage;   ///< Current installation stage
    InstallationState currentState;   ///< Current state machine state

    InstallationProgress()
        : bytesWritten(0)
        , totalBytes(0)
        , startTimeMs(0)
        , lastUpdateMs(0)
        , currentStage(InstallationStage::NotStarted)
        , currentState(InstallationState::Idle) {}

    /**
     * @brief Get current completion percentage
     */
    uint8_t percentage() const {
        if (totalBytes == 0) return 0;
        return static_cast<uint8_t>((static_cast<uint64_t>(bytesWritten) * 100) / totalBytes);
    }

    /**
     * @brief Get elapsed time in milliseconds
     */
    uint32_t elapsedTimeMs() const {
        if (startTimeMs == 0) return 0;
        return static_cast<uint32_t>(millis() - startTimeMs);
    }
};

/**
 * @brief Verified payload descriptor - immutable descriptor of verified firmware to install
 */
struct VerifiedPayloadDescriptor {
    const void* payloadPointer;       ///< Pointer to payload data (or stream source)
    uint32_t payloadSize;             ///< Total size of the firmware payload
    uint32_t expectedVersionCode;     ///< Version code of the firmware
    const char* expectedVersionString;///< Version string of the firmware
    bool isCompressed;                ///< Whether payload is compressed

    VerifiedPayloadDescriptor()
        : payloadPointer(nullptr)
        , payloadSize(0)
        , expectedVersionCode(0)
        , expectedVersionString("")
        , isCompressed(false) {}
};

/**
 * @brief OTA session state - strongly typed lifecycle states for the ESP-IDF OTA session
 * 
 * Used internally by InstallationHandle to track the exact state of the OTA session.
 * Prevents invalid operations like calling esp_ota_abort() on a closed session.
 */
enum class OtaSessionState : uint8_t {
    Invalid,            ///< No session exists, handle is uninitialized
    PartitionSelected,  ///< OTA partition has been discovered and selected
    SessionOpen,        ///< ESP-IDF OTA session is actively open (esp_ota_begin() succeeded)
    SessionClosed       ///< OTA session has been closed (either via abort or cleanup)
};

/**
 * @brief Installation handle - encapsulates ESP-IDF OTA session resources
 * 
 * This handle contains:
 * - OTA session handle for ESP-IDF OTA operations (stored as opaque uint32_t)
 * - Partition pointer to the target OTA partition (stored as opaque uintptr_t)
 * - Strongly typed session state to track lifecycle
 * 
 * Phase 6B.1 fix: Uses opaque storage to avoid ESP-IDF type leaks into public headers
 * No ESP-IDF types are exposed in this public header
 */
struct InstallationHandle {
    // Opaque storage for ESP-IDF types - maintains perfect platform independence
    uint32_t _otaHandle;              ///< Opaque storage for esp_ota_handle_t (32-bit value)
    uintptr_t _partition;             ///< Opaque storage for const esp_partition_t* (pointer-sized integer)
    OtaSessionState _sessionState;    ///< Current state of the OTA session

    InstallationHandle()
        : _otaHandle(0)
        , _partition(0)
        , _sessionState(OtaSessionState::Invalid) {}
    
    /**
     * @brief Check if session can be aborted (only valid in SessionOpen state)
     */
    bool canAbort() const {
        return _sessionState == OtaSessionState::SessionOpen;
    }
    
    /**
     * @brief Check if session is actively open
     */
    bool isOpen() const {
        return _sessionState == OtaSessionState::SessionOpen;
    }
    
    /**
     * @brief Check if partition has been selected
     */
    bool hasPartition() const {
        return _sessionState >= OtaSessionState::PartitionSelected && _partition != 0;
    }
};

/**
 * @brief Installation session - lightweight unique session tracking
 * Fully POD, zero heap allocations, tracks per-installation metadata
 * Phase 6B.1 audit: Uses strongly typed OtaSessionState from InstallationHandle
 */
struct InstallationSession {
    uint64_t sessionId;               ///< Unique 64-bit identifier for this installation
    uint64_t creationTimestampMs;     ///< When this session was created
    bool isActive;                    ///< Whether this session is currently active
    uint32_t chunksReceived;          ///< Number of data chunks received
    uint32_t chunksWritten;           ///< Number of data chunks successfully written
    uint32_t lastChunkSize;           ///< Size of the last written chunk
    uint64_t lastWriteTimestampMs;    ///< Timestamp of last successful write
    InstallationHandle otaHandle;     ///< ESP-IDF OTA session handle with lifecycle tracking

    InstallationSession()
        : sessionId(0)
        , creationTimestampMs(0)
        , isActive(false)
        , chunksReceived(0)
        , chunksWritten(0)
        , lastChunkSize(0)
        , lastWriteTimestampMs(0) {}
};

/**
 * @brief Installation context - everything needed to begin an installation
 */
struct InstallationContext {
    Verification::VerificationResult verificationResult;  ///< Complete verification proof
    VerifiedPayloadDescriptor payloadDescriptor;         ///< Description of the payload
    InstallationPolicy policy;                           ///< Installation policy to apply
    uint32_t targetPartitionId;                          ///< Optional: specific partition to use

    InstallationContext()
        : targetPartitionId(0) {}

    /**
     * @brief Validate that the context is valid for installation
     */
    bool isValid() const {
        return verificationResult.eligibleForInstallation &&
               payloadDescriptor.payloadPointer != nullptr &&
               payloadDescriptor.payloadSize > 0;
    }
};

/**
 * @brief Installation warning - non-fatal issues during installation
 */
enum class InstallationWarning : uint8_t {
    None,
    SlowWritePerformance,     ///< Write speeds are lower than expected
    PartitionAlmostFull,      ///< Target partition is nearly full
    LegacyPartitionFormat,    ///< Using older partition format
    DowngradeAllowed          ///< Version downgrade was permitted by policy
};

/**
 * @brief Installation result - final outcome of an installation attempt
 */
struct InstallationResult {
    InstallationStatus status;
    InstallationError error;
    InstallationProgress finalProgress;
    InstallationSession session;
    std::array<InstallationWarning, 4> warnings;
    uint8_t warningCount;
    const char* errorMessage;
    uint32_t targetPartitionId;  ///< Partition that was targeted (placeholder)
    bool bootPartitionUpdated;   ///< Whether boot partition was updated (placeholder)

    InstallationResult()
        : status(InstallationStatus::NotStarted)
        , error(InstallationError::None)
        , warningCount(0)
        , errorMessage("")
        , targetPartitionId(0)
        , bootPartitionUpdated(false) {
        warnings.fill(InstallationWarning::None);
    }

    /**
     * @brief Add a warning to the result
     */
    void addWarning(InstallationWarning warning) {
        if (warningCount < warnings.size()) {
            warnings[warningCount++] = warning;
        }
    }

    /**
     * @brief Check if installation completed successfully
     */
    bool isSuccess() const {
        return status == InstallationStatus::Success && error == InstallationError::None;
    }
};

/**
 * @brief Result of explicitly activating an already-installed firmware image
 *
 * Lightweight POD result for Phase 6B.4. The installer remains the owner of
 * activation; callers must request it explicitly after a completed install.
 */
struct ActivationResult {
    bool success;
    InstallationError error;
    const char* message;
    bool rebootRequired;
    bool bootPartitionChanged;
    uint64_t activationTimestampMs;
};

/**
 * @brief Convert state to string for diagnostics
 */
inline const char* stateToString(InstallationState state) {
    switch (state) {
        case InstallationState::Idle: return "Idle";
        case InstallationState::Initializing: return "Initializing";
        case InstallationState::Ready: return "Ready";
        case InstallationState::Preparing: return "Preparing";
        case InstallationState::Installing: return "Installing";
        case InstallationState::Finalizing: return "Finalizing";
        case InstallationState::Completed: return "Completed";
        case InstallationState::Aborted: return "Aborted";
        case InstallationState::Error: return "Error";
        default: return "Unknown";
    }
}

/**
 * @brief Convert error to string for diagnostics
 */
inline const char* errorToString(InstallationError error) {
    switch (error) {
        case InstallationError::None: return "None";
        case InstallationError::NotInitialized: return "NotInitialized";
        case InstallationError::Busy: return "Busy";
        case InstallationError::InvalidContext: return "InvalidContext";
        case InstallationError::InvalidState: return "InvalidState";
        case InstallationError::Cancelled: return "Cancelled";
        case InstallationError::WriteFailed: return "WriteFailed";
        case InstallationError::PartitionUnavailable: return "PartitionUnavailable";
        case InstallationError::FinalizeFailed: return "FinalizeFailed";
        case InstallationError::InternalError: return "InternalError";
        case InstallationError::Unsupported: return "Unsupported";
        default: return "Unknown";
    }
}

} // namespace Installation
