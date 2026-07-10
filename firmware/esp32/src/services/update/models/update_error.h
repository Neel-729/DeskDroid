#pragma once

#include <Arduino.h>
#include <string>

/**
 * @brief Enumeration of all possible update error categories and codes
 * 
 * Provides type-safe error classification for the OTA update system,
 * covering all failure modes across networking, validation, versioning,
 * compatibility, security, and system operations.
 */
enum class UpdateErrorCode {
    // Generic success
    None = 0,
    
    // Network errors (100-199)
    NetworkConnectionFailed = 100,
    NetworkTimeout = 101,
    NetworkTLSFailed = 102,
    NetworkHttpError = 103,
    NetworkDnsResolutionFailed = 104,
    
    // Validation errors (200-299)
    ValidationInvalidInput = 200,
    ValidationIncompleteData = 201,
    ValidationInvalidFormat = 202,
    ValidationCorruptedData = 203,
    
    // Version errors (300-399)
    VersionInvalidFormat = 300,
    VersionParseFailed = 301,
    VersionNotNewer = 302,
    VersionSameAsCurrent = 303,
    
    // Compatibility errors (400-499)
    CompatibilityHardwareMismatch = 400,
    CompatibilityMinimumVersionNotMet = 401,
    CompatibilityProtocolMismatch = 402,
    CompatibilityPartitionTooSmall = 403,
    
    // Security errors (500-599)
    SecurityInvalidSignature = 500,
    SecurityInvalidCertificate = 501,
    SecurityHashMismatch = 502,
    SecurityUnsecureConnection = 503,
    
    // System errors (600-699)
    SystemStorageFull = 600,
    SystemIoError = 601,
    SystemMemoryAllocationFailed = 602,
    SystemPartitionNotFound = 603,
    SystemFlashOperationFailed = 604,
    
    // Update state errors (700-799)
    StateInvalidTransition = 700,
    StateOperationNotPermitted = 701,
    StateUpdateInProgress = 702
};

/**
 * @brief Immutable error container for update system errors
 * 
 * Wraps an error code with a human-readable message for detailed error
 * reporting and debugging. Provides helper methods for error classification.
 */
class UpdateError {
public:
    /**
     * @brief Create a successful (no error) instance
     */
    UpdateError() : _code(UpdateErrorCode::None), _message("Success") {}
    
    /**
     * @brief Create an error with specific code and message
     * @param code The error code
     * @param message Human-readable error description
     */
    UpdateError(UpdateErrorCode code, const std::string& message) 
        : _code(code), _message(message) {}
    
    /**
     * @brief Check if this represents a successful operation
     * @return true if no error occurred
     */
    bool isSuccess() const { return _code == UpdateErrorCode::None; }
    
    /**
     * @brief Check if this represents an error condition
     * @return true if an error occurred
     */
    bool hasError() const { return _code != UpdateErrorCode::None; }
    
    /**
     * @brief Get the error code
     * @return The UpdateErrorCode value
     */
    UpdateErrorCode code() const { return _code; }
    
    /**
     * @brief Get the human-readable error message
     * @return Const reference to the error message string
     */
    const std::string& message() const { return _message; }
    
    /**
     * @brief Helper to create a success instance
     * @return UpdateError with no error
     */
    static UpdateError ok() { return UpdateError(); }
    
    /**
     * @brief Create an error from code and message
     * @param code The error code
     * @param message Human-readable description
     * @return UpdateError instance
     */
    static UpdateError create(UpdateErrorCode code, const std::string& message) {
        return UpdateError(code, message);
    }
    
private:
    UpdateErrorCode _code;
    std::string _message;
};