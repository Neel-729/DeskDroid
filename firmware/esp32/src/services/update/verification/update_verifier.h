#pragma once

#include <Arduino.h>
#include "iverifier.h"
#include "verification_types.h"
#include "../models/update_validator.h"
#include "../firmware_info.h"

namespace Verification {

/**
 * @brief Primary implementation of the IVerifier interface
 * 
 * UpdateVerifier implements the complete verification pipeline as defined
 * in Phase 5. It evaluates payloads and metadata against the current
 * firmware and the active verification policy to determine if an update
 * is eligible for installation.
 * 
 * This implementation is passive, stateless between verification runs,
 * and maintains strict separation of concerns from transport and installer
 * layers.
 */
class UpdateVerifier : public IVerifier {
public:
    /**
     * @brief Construct a new UpdateVerifier with default configuration
     */
    UpdateVerifier();

    /**
     * @brief Construct a new UpdateVerifier with a specific policy
     * @param policy The verification policy to enforce
     */
    explicit UpdateVerifier(const VerificationPolicy& policy);

    // IVerifier interface implementation
    bool initialize() override;
    void shutdown() override;
    bool isReady() const override;
    VerificationResult verify(const VerificationContext& context) override;
    const VerificationCapabilities& getCapabilities() const override;
    const VerificationResult& getLastResult() const override;

    /**
     * @brief Set the active verification policy
     * @param policy New policy to enforce for future verifications
     */
    void setPolicy(const VerificationPolicy& policy);

    /**
     * @brief Get the currently active verification policy
     * @return Const reference to the current policy
     */
    const VerificationPolicy& getPolicy() const;

private:
    bool _initialized;
    VerificationPolicy _activePolicy;
    VerificationCapabilities _capabilities;
    VerificationResult _lastResult;

    // Pipeline stage methods - each handles one verification step
    StageResult validateMetadata(const VerificationContext& context, VerificationDetails& details);
    StageResult validateSize(const VerificationContext& context, VerificationDetails& details);
    StageResult validateCompatibility(const VerificationContext& context, VerificationDetails& details);
    StageResult validateIntegrity(const VerificationContext& context, VerificationDetails& details);
    StageResult validateAuthenticity(const VerificationContext& context, VerificationDetails& details);

    /**
     * @brief Check if a verification stage should be executed based on policy
     * @param stage The stage to check
     * @param policy The active policy
     * @return true if the stage should run, false if it should be skipped
     */
    bool shouldRunStage(VerificationStage stage, const VerificationPolicy& policy) const;

    /**
     * @brief Build the final eligibility decision based on all stage results
     * @param details The completed verification details
     * @param error Any error that occurred during verification
     * @param context The verification context that was evaluated
     * @return true if the payload is eligible for installation
     */
    bool evaluateEligibility(const VerificationDetails& details, VerificationError error, const VerificationContext& context) const;

    /**
     * @brief Check if hardware variant is allowed by policy
     */
    bool isHardwareVariantAllowed(const char* variant, const VerificationPolicy& policy) const;
    bool isHardwareVariantAllowed(const std::string& variant, const VerificationPolicy& policy) const;

    /**
     * @brief Internal method to reset state before a new verification run
     */
    void resetForNewVerification();

#ifdef OTA_PLATFORM_VALIDATION
    /**
     * @brief Debug logging for verification pipeline (only in validation builds)
     */
    void logStageResult(const StageResult& result, const char* stageName) const;
#endif
};

} // namespace Verification