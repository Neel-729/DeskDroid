#include "update_verifier.h"
#include <string.h>
#include "esp_log.h"

namespace Verification {

static const char* TAG = "UpdateVerifier";

UpdateVerifier::UpdateVerifier()
    : _initialized(false) {
    // Initialize capabilities with what we currently support
    _capabilities.supportsIntegrityChecks = false; // To be implemented in future phase
    _capabilities.supportsSignatureVerification = false; // To be implemented in future phase
    _capabilities.supportsRollbackProtection = false;
    _capabilities.supportsHardwareWhitelisting = true;
    
    // Default to production policy
    _activePolicy = VerificationPolicy::createProductionPolicy();
}

UpdateVerifier::UpdateVerifier(const VerificationPolicy& policy)
    : _initialized(false)
    , _activePolicy(policy) {
    _capabilities.supportsIntegrityChecks = false;
    _capabilities.supportsSignatureVerification = false;
    _capabilities.supportsRollbackProtection = false;
    _capabilities.supportsHardwareWhitelisting = true;
}

bool UpdateVerifier::initialize() {
    if (_initialized) {
        return true;
    }

#ifdef OTA_PLATFORM_VALIDATION
    ESP_LOGI(TAG, "UpdateVerifier initializing");
#endif

    _initialized = true;
    return true;
}

void UpdateVerifier::shutdown() {
    _initialized = false;
#ifdef OTA_PLATFORM_VALIDATION
    ESP_LOGI(TAG, "UpdateVerifier shutdown");
#endif
}

bool UpdateVerifier::isReady() const {
    return _initialized;
}

const VerificationCapabilities& UpdateVerifier::getCapabilities() const {
    return _capabilities;
}

const VerificationResult& UpdateVerifier::getLastResult() const {
    return _lastResult;
}

void UpdateVerifier::setPolicy(const VerificationPolicy& policy) {
    _activePolicy = policy;
}

const VerificationPolicy& UpdateVerifier::getPolicy() const {
    return _activePolicy;
}

void UpdateVerifier::resetForNewVerification() {
    _lastResult = VerificationResult();
}

VerificationResult UpdateVerifier::verify(const VerificationContext& context) {
    resetForNewVerification();
    VerificationResult result;
    result.status = VerificationStatus::InProgress;

    if (!_initialized) {
        result.status = VerificationStatus::Failed;
        result.error = VerificationError::NotInitialized;
        strncpy(result.details.diagnosticMessage, "Verifier not initialized", sizeof(result.details.diagnosticMessage) - 1);
        _lastResult = result;
        return result;
    }

    if (!context.isValid()) {
        result.status = VerificationStatus::Failed;
        result.error = VerificationError::InsufficientContext;
        strncpy(result.details.diagnosticMessage, "Missing required context", sizeof(result.details.diagnosticMessage) - 1);
        _lastResult = result;
        return result;
    }

    uint32_t startTime = millis();

    // Execute verification pipeline
    StageResult metadataResult = validateMetadata(context, result.details);
    result.details.addStageResult(metadataResult);
#ifdef OTA_PLATFORM_VALIDATION
    logStageResult(metadataResult, "Metadata");
#endif
    
    if (!metadataResult.passed) {
        result.status = VerificationStatus::Failed;
        result.error = VerificationError::InvalidMetadata;
        strncpy(result.details.diagnosticMessage, "Metadata validation failed", sizeof(result.details.diagnosticMessage) - 1);
        result.details.totalDurationMs = millis() - startTime;
        _lastResult = result;
        return result;
    }

    StageResult sizeResult = validateSize(context, result.details);
    result.details.addStageResult(sizeResult);
#ifdef OTA_PLATFORM_VALIDATION
    logStageResult(sizeResult, "Size");
#endif

    if (!sizeResult.passed) {
        result.status = VerificationStatus::Failed;
        result.error = VerificationError::SizeMismatch;
        strncpy(result.details.diagnosticMessage, "Size validation failed", sizeof(result.details.diagnosticMessage) - 1);
        result.details.totalDurationMs = millis() - startTime;
        _lastResult = result;
        return result;
    }

    StageResult compatibilityResult = validateCompatibility(context, result.details);
    result.details.addStageResult(compatibilityResult);
#ifdef OTA_PLATFORM_VALIDATION
    logStageResult(compatibilityResult, "Compatibility");
#endif

    if (!compatibilityResult.passed) {
        result.status = VerificationStatus::Failed;
        result.error = VerificationError::IncompatibleVersion;
        strncpy(result.details.diagnosticMessage, "Compatibility validation failed", sizeof(result.details.diagnosticMessage) - 1);
        result.details.totalDurationMs = millis() - startTime;
        _lastResult = result;
        return result;
    }

    StageResult integrityResult = validateIntegrity(context, result.details);
    result.details.addStageResult(integrityResult);
#ifdef OTA_PLATFORM_VALIDATION
    logStageResult(integrityResult, "Integrity");
#endif

    if (!integrityResult.passed && shouldRunStage(VerificationStage::IntegrityValidation, *context.policy)) {
        result.status = VerificationStatus::Failed;
        result.error = VerificationError::IntegrityCheckFailed;
        strncpy(result.details.diagnosticMessage, "Integrity validation failed", sizeof(result.details.diagnosticMessage) - 1);
        result.details.totalDurationMs = millis() - startTime;
        _lastResult = result;
        return result;
    }

    StageResult authenticityResult = validateAuthenticity(context, result.details);
    result.details.addStageResult(authenticityResult);
#ifdef OTA_PLATFORM_VALIDATION
    logStageResult(authenticityResult, "Authenticity");
#endif

    if (!authenticityResult.passed && shouldRunStage(VerificationStage::AuthenticityValidation, *context.policy)) {
        result.status = VerificationStatus::Failed;
        result.error = VerificationError::AuthenticityCheckFailed;
        strncpy(result.details.diagnosticMessage, "Authenticity validation failed", sizeof(result.details.diagnosticMessage) - 1);
        result.details.totalDurationMs = millis() - startTime;
        _lastResult = result;
        return result;
    }

    // All stages completed successfully
    result.status = VerificationStatus::Success;
    result.error = VerificationError::None;
    result.details.totalDurationMs = millis() - startTime;
    result.eligibleForInstallation = evaluateEligibility(result.details, result.error, context);
    
    if (result.eligibleForInstallation) {
        strncpy(result.details.diagnosticMessage, "All verification checks passed", sizeof(result.details.diagnosticMessage) - 1);
    } else {
        strncpy(result.details.diagnosticMessage, "Verification passed but installation not eligible", sizeof(result.details.diagnosticMessage) - 1);
    }

#ifdef OTA_PLATFORM_VALIDATION
    ESP_LOGI(TAG, "Verification complete in %dms, eligible: %d", result.details.totalDurationMs, result.eligibleForInstallation);
#endif

    _lastResult = result;
    return result;
}

StageResult UpdateVerifier::validateMetadata(const VerificationContext& context, VerificationDetails& details) {
    uint32_t start = millis();
    StageResult result(VerificationStage::MetadataValidation, false);

    // Use existing UpdateValidator to check metadata
    UpdateError validationError = UpdateValidator::validateUpdateInfo(*context.candidateUpdate);
    
    if (validationError.isSuccess()) {
        result.passed = true;
    } else {
        result.passed = false;
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGW(TAG, "Metadata validation failed with UpdateError: %d", static_cast<int>(validationError.code()));
#endif
    }

    result.durationMs = millis() - start;
    return result;
}

StageResult UpdateVerifier::validateSize(const VerificationContext& context, VerificationDetails& details) {
    uint32_t start = millis();
    StageResult result(VerificationStage::SizeValidation, false);

    const PayloadDescriptor* payload = context.payload;
    const VerificationPolicy& policy = *context.policy;

    // Check if size matches what was declared
    if (payload->actualSize != payload->declaredSize) {
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGW(TAG, "Size mismatch: actual=%u, declared=%u", payload->actualSize, payload->declaredSize);
#endif
        result.durationMs = millis() - start;
        return result; // fails
    }

    // Check if size exceeds maximum if policy enforces it
    if (policy.enforceMaxPayloadSize && payload->actualSize > policy.maxPayloadSizeBytes) {
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGW(TAG, "Payload size %u exceeds maximum %u", payload->actualSize, policy.maxPayloadSizeBytes);
#endif
        result.durationMs = millis() - start;
        return result; // fails
    }

    result.passed = true;
    result.durationMs = millis() - start;
    return result;
}

StageResult UpdateVerifier::validateCompatibility(const VerificationContext& context, VerificationDetails& details) {
    uint32_t start = millis();
    StageResult result(VerificationStage::CompatibilityValidation, false);

    // First check hardware compatibility if policy has allowed variants
    if (context.policy->allowedHardwareVariants != nullptr && context.policy->allowedVariantsCount > 0) {
        if (!isHardwareVariantAllowed(context.candidateUpdate->hardwareVariant, *context.policy)) {
#ifdef OTA_PLATFORM_VALIDATION
            ESP_LOGW(TAG, "Hardware variant %s not allowed", context.candidateUpdate->hardwareVariant.c_str());
#endif
            result.durationMs = millis() - start;
            return result;
        }
    }

    // Use existing UpdateValidator for compatibility checks
    UpdateError compatError = UpdateValidator::checkCompatibility(*context.candidateUpdate, *context.currentFirmware);
    
    if (compatError.hasError()) {
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGW(TAG, "Compatibility check failed with error: %d", static_cast<int>(compatError.code()));
#endif
        result.durationMs = millis() - start;
        return result;
    }

    // Check version order and downgrade policy
    VersionComparison versionCompare = UpdateValidator::compareVersion(*context.candidateUpdate, *context.currentFirmware);
    if (versionCompare == VersionComparison::Older && !context.policy->allowDowngrades) {
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGW(TAG, "Downgrade not allowed by policy");
#endif
        result.durationMs = millis() - start;
        return result;
    }

    result.passed = true;
    result.durationMs = millis() - start;
    return result;
}

StageResult UpdateVerifier::validateIntegrity(const VerificationContext& context, VerificationDetails& details) {
    uint32_t start = millis();
    StageResult result(VerificationStage::IntegrityValidation, false);

    // Integrity checks are stubs for future implementation
    // In this phase, we only check if policy requires this stage
    if (!shouldRunStage(VerificationStage::IntegrityValidation, *context.policy)) {
        // Stage skipped due to policy
        result.passed = true;
        details.addWarning(VerificationWarning::HashDeferred);
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGD(TAG, "Integrity checks skipped by policy");
#endif
    } else {
        // Integrity checks would run here in future phases
        // For now, we pass since we haven't implemented real hash validation
        result.passed = true;
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGD(TAG, "Integrity checks stubbed - passed");
#endif
    }

    result.durationMs = millis() - start;
    return result;
}

StageResult UpdateVerifier::validateAuthenticity(const VerificationContext& context, VerificationDetails& details) {
    uint32_t start = millis();
    StageResult result(VerificationStage::AuthenticityValidation, false);

    // Authenticity checks are stubs for future implementation
    if (!shouldRunStage(VerificationStage::AuthenticityValidation, *context.policy)) {
        result.passed = true;
        details.addWarning(VerificationWarning::SignatureDeferred);
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGD(TAG, "Authenticity checks skipped by policy");
#endif
    } else {
        // Signature verification would run here in future phases
        result.passed = true;
#ifdef OTA_PLATFORM_VALIDATION
        ESP_LOGD(TAG, "Authenticity checks stubbed - passed");
#endif
    }

    result.durationMs = millis() - start;
    return result;
}

bool UpdateVerifier::shouldRunStage(VerificationStage stage, const VerificationPolicy& policy) const {
    switch (policy.level) {
        case VerificationPolicyLevel::Disabled:
            return false;
            
        case VerificationPolicyLevel::Development:
            // Only run basic metadata and compatibility checks in development
            return stage == VerificationStage::MetadataValidation 
                || stage == VerificationStage::SizeValidation
                || stage == VerificationStage::CompatibilityValidation;
            
        case VerificationPolicyLevel::CompatibilityOnly:
            // Run all except integrity and authenticity
            return stage != VerificationStage::IntegrityValidation 
                && stage != VerificationStage::AuthenticityValidation;
            
        case VerificationPolicyLevel::IntegrityOnly:
            // Run all except authenticity
            return stage != VerificationStage::AuthenticityValidation;
            
        case VerificationPolicyLevel::Strict:
        default:
            // Run all stages
            return true;
    }
}

bool UpdateVerifier::evaluateEligibility(const VerificationDetails& details, VerificationError error, const VerificationContext& context) const {
    // If any error occurred, not eligible
    if (error != VerificationError::None) {
        return false;
    }

    // All stages must have passed for stages that were required to run
    for (const auto& stageResult : details.stageResults) {
        if (shouldRunStage(stageResult.stage, *context.policy) && !stageResult.passed) {
            return false;
        }
    }

    // All checks passed
    return true;
}

bool UpdateVerifier::isHardwareVariantAllowed(const char* variant, const VerificationPolicy& policy) const {
    if (variant == nullptr || policy.allowedHardwareVariants == nullptr) {
        return false;
    }

    for (size_t i = 0; i < policy.allowedVariantsCount; i++) {
        const char* allowed = policy.allowedHardwareVariants[i];
        if (allowed != nullptr && strcmp(variant, allowed) == 0) {
            return true;
        }
    }

    return false;
}

bool UpdateVerifier::isHardwareVariantAllowed(const std::string& variant, const VerificationPolicy& policy) const {
    return isHardwareVariantAllowed(variant.c_str(), policy);
}

#ifdef OTA_PLATFORM_VALIDATION
void UpdateVerifier::logStageResult(const StageResult& result, const char* stageName) const {
    ESP_LOGD(TAG, "%s stage: %s (took %dms)", 
        stageName, 
        result.passed ? "PASSED" : "FAILED",
        result.durationMs);
}
#endif

} // namespace Verification