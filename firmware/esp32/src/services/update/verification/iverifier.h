#pragma once

#include <Arduino.h>
#include "verification_types.h"

namespace Verification {

/**
 * @brief Interface for all verifier implementations
 * 
 * Defines the contract that all verification engines must implement.
 * This abstract base class enables swapping verification implementations
 * without changing the core UpdateManager orchestration logic.
 */
class IVerifier {
public:
    /**
     * @brief Initialize the verifier
     * @return true if initialization succeeded
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the verifier and release resources
     */
    virtual void shutdown() = 0;

    /**
     * @brief Check if verifier is ready to perform verification
     * @return true if verifier is initialized and ready
     */
    virtual bool isReady() const = 0;

    /**
     * @brief Perform verification on the provided context
     * @param context Complete verification context containing all inputs
     * @return Complete verification result with status, errors, and details
     */
    virtual VerificationResult verify(const VerificationContext& context) = 0;

    /**
     * @brief Get the capabilities supported by this verifier
     * @return Const reference to capabilities structure
     */
    virtual const VerificationCapabilities& getCapabilities() const = 0;

    /**
     * @brief Get the last verification result
     * @return Const reference to the most recent verification result
     */
    virtual const VerificationResult& getLastResult() const = 0;

    /**
     * @brief Virtual destructor for proper polymorphism
     */
    virtual ~IVerifier() = default;

protected:
    /**
     * @brief Protected constructor - only derived classes can instantiate
     */
    IVerifier() = default;
};

} // namespace Verification