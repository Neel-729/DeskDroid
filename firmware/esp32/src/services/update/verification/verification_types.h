#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <vector>
#include <string>
#include "../models/update_error.h"
#include "../models/update_info.h"
#include "../firmware_info.h"

namespace Verification {

/**
 * @brief Verification status codes - strongly typed enumeration of all possible verification outcomes
 */
enum class VerificationStatus : uint8_t {
    Idle,               ///< Verification not yet started
    InProgress,        ///< Verification is currently running
    Success,           ///< All checks passed
    Failed,            ///< One or more checks failed
    Skipped,           ///< Verification skipped due to policy
    Deferred           ///< Verification deferred to a later stage
};

/**
 * @brief Verification error codes - detailed error classification
 */
enum class VerificationError : uint8_t {
    None,              ///< No error
    NotInitialized,    ///< Verifier not initialized
    InvalidMetadata,   ///< Metadata is malformed or missing required fields
    IncompatibleHardware, ///< Hardware variant mismatch
    IncompatibleVersion,  ///< Version requirements not met
    SizeMismatch,      ///< Declared size doesn't match actual payload size
    SizeExceeded,      ///< Payload size exceeds maximum allowed
    IntegrityCheckFailed, ///< Integrity validation failed
    AuthenticityCheckFailed, ///< Signature/trust validation failed
    PolicyViolation,   ///< Policy requirements not satisfied
    InsufficientContext, ///< Missing required context information
    InternalError,     ///< Internal verification error
    Unsupported        ///< Operation not supported by current verifier
};

/**
 * @brief Verification warning codes - non-fatal issues that don't block installation
 */
enum class VerificationWarning : uint8_t {
    None,              ///< No warnings
    DebugBuildAllowed, ///< Debug firmware allowed in development policy
    SignatureDeferred, ///< Signature verification deferred
    HashDeferred,      ///< Hash verification deferred
    BetaVersionAllowed,///< Beta firmware accepted in policy
    MinimumVersionMarginLow ///< Current version is close to minimum required
};

/**
 * @brief Verification policy levels - defines strictness of verification
 */
enum class VerificationPolicyLevel : uint8_t {
    Disabled,          ///< All verification checks disabled
    Development,       ///< Development mode - minimal checks, allow debug builds
    CompatibilityOnly, ///< Only compatibility checks, no integrity/authenticity
    IntegrityOnly,     ///< Compatibility + integrity checks
    Strict             ///< All checks enabled (production default)
};

/**
 * @brief Stage of verification pipeline - tracks progress through verification steps
 */
enum class VerificationStage : uint8_t {
    NotStarted,
    MetadataValidation,
    SizeValidation,
    CompatibilityValidation,
    IntegrityValidation,
    AuthenticityValidation,
    Complete
};

/**
 * @brief Stage summary - records result of an individual verification stage
 */
struct StageResult {
    VerificationStage stage;
    bool passed;
    uint32_t durationMs; ///< Time taken to complete this stage
    const char* message; ///< Optional diagnostic message

    StageResult(VerificationStage s, bool p) 
        : stage(s), passed(p), durationMs(0), message("") {}
};

/**
 * @brief Verification capabilities - describes what this verifier supports
 */
struct VerificationCapabilities {
    bool supportsIntegrityChecks : 1;
    bool supportsSignatureVerification : 1;
    bool supportsRollbackProtection : 1;
    bool supportsHardwareWhitelisting : 1;

    VerificationCapabilities()
        : supportsIntegrityChecks(false)
        , supportsSignatureVerification(false)
        , supportsRollbackProtection(false)
        , supportsHardwareWhitelisting(false) {}
};

/**
 * @brief Verification policy - configuration for how verification should behave
 */
struct VerificationPolicy {
    VerificationPolicyLevel level;
    bool allowDowngrades : 1;
    bool allowDebugBuilds : 1;
    bool allowBetaVersions : 1;
    bool enforceMaxPayloadSize : 1;
    uint32_t maxPayloadSizeBytes; ///< Maximum allowed payload size if enforced
    const char* const* allowedHardwareVariants; ///< Null-terminated list of allowed variants
    size_t allowedVariantsCount; ///< Number of allowed hardware variants

    VerificationPolicy()
        : level(VerificationPolicyLevel::Strict)
        , allowDowngrades(false)
        , allowDebugBuilds(false)
        , allowBetaVersions(false)
        , enforceMaxPayloadSize(true)
        , maxPayloadSizeBytes(1024 * 1024 * 8) ///< 8MB default
        , allowedHardwareVariants(nullptr)
        , allowedVariantsCount(0) {}

    /**
     * @brief Create a development-friendly policy with relaxed checks
     */
    static VerificationPolicy createDevelopmentPolicy() {
        VerificationPolicy p;
        p.level = VerificationPolicyLevel::Development;
        p.allowDowngrades = true;
        p.allowDebugBuilds = true;
        p.allowBetaVersions = true;
        return p;
    }

    /**
     * @brief Create a strict production policy
     */
    static VerificationPolicy createProductionPolicy() {
        VerificationPolicy p;
        p.level = VerificationPolicyLevel::Strict;
        p.allowDowngrades = false;
        p.allowDebugBuilds = false;
        p.allowBetaVersions = false;
        return p;
    }
};

/**
 * @brief Payload descriptor - describes the actual payload received from transport
 */
struct PayloadDescriptor {
    const void* data; ///< Pointer to payload data (non-owning)
    uint32_t actualSize; ///< Actual size of payload received
    uint32_t declaredSize; ///< Size declared in metadata
    const char* expectedHash; ///< Optional hash from metadata
    const char* expectedSignature; ///< Optional signature from metadata
    bool isComplete; ///< Whether the payload is fully received

    PayloadDescriptor()
        : data(nullptr)
        , actualSize(0)
        , declaredSize(0)
        , expectedHash(nullptr)
        , expectedSignature(nullptr)
        , isComplete(false) {}
};

/**
 * @brief Verification context - all input data needed to perform verification
 */
struct VerificationContext {
    const FirmwareInfo* currentFirmware; ///< Currently running firmware info (non-owning)
    const UpdateInfo* candidateUpdate; ///< Candidate update metadata (non-owning)
    const PayloadDescriptor* payload; ///< Payload descriptor (non-owning)
    const VerificationPolicy* policy; ///< Verification policy to apply (non-owning)
    void* reserved; ///< Reserved for future extension

    VerificationContext()
        : currentFirmware(nullptr)
        , candidateUpdate(nullptr)
        , payload(nullptr)
        , policy(nullptr)
        , reserved(nullptr) {}

    /**
     * @brief Check if all required context fields are present
     */
    bool isValid() const {
        return currentFirmware != nullptr 
            && candidateUpdate != nullptr 
            && payload != nullptr 
            && policy != nullptr;
    }
};

/**
 * @brief Verification details - detailed results and diagnostics from verification
 */
struct VerificationDetails {
    std::vector<StageResult> stageResults; ///< Results from each verification stage
    std::vector<VerificationWarning> warnings; ///< Non-fatal warnings
    uint32_t totalDurationMs; ///< Total time taken for all verification
    char diagnosticMessage[128]; ///< Human-readable diagnostic message

    VerificationDetails()
        : totalDurationMs(0) {
        diagnosticMessage[0] = '\0';
    }

    /**
     * @brief Add a stage result to the details
     */
    void addStageResult(const StageResult& result) {
        stageResults.push_back(result);
    }

    /**
     * @brief Add a warning to the details
     */
    void addWarning(VerificationWarning warning) {
        if (warning != VerificationWarning::None) {
            warnings.push_back(warning);
        }
    }
};

/**
 * @brief Complete verification result - returned from verifier.verify()
 */
struct VerificationResult {
    VerificationStatus status;
    VerificationError error;
    VerificationDetails details;
    bool eligibleForInstallation; ///< Whether the payload is safe to install

    VerificationResult()
        : status(VerificationStatus::Idle)
        , error(VerificationError::None)
        , eligibleForInstallation(false) {}

    /**
     * @brief Check if verification completed successfully
     */
    bool isSuccess() const {
        return status == VerificationStatus::Success && error == VerificationError::None;
    }
};

/**
 * @brief Verified artifact - placeholder for future immutable verified payload
 * 
 * This type is reserved for future phases when we'll wrap verified payloads
 * to ensure only verified data can be passed to the installer.
 */
struct VerifiedArtifact {
    const void* data;
    uint32_t size;
    VerificationResult verificationProof;

    VerifiedArtifact() : data(nullptr), size(0) {}
};

} // namespace Verification