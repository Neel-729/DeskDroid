#pragma once

#include <Arduino.h>
#include <string>
#include "update_error.h"
#include "update_info.h"
#include "../firmware_info.h"
#include "../version_compare.h"
#include <cstdint>
#include <algorithm>

/**
 * @brief Pure stateless validation engine for update metadata
 * 
 * Provides comprehensive validation of UpdateInfo structures against
 * the currently running firmware and system constraints. All methods
 * are static and thread-safe, making the validator usable in any context.
 * 
 * This centralizes all validation logic to ensure consistent update
 * evaluation across all update providers and system components.
 */
class UpdateValidator {
public:
    /**
     * @brief Validate a complete UpdateInfo structure
     * @param info The UpdateInfo to validate
     * @return UpdateError with error status (success if valid)
     * 
     * Performs all available validation checks in sequence:
     * 1. Basic sanity checks for required fields
     * 2. Version format and numeric validation
     * 3. URL format validation
     * 4. SHA256 hash format validation
     * 5. File size sanity checks
     */
    static UpdateError validateUpdateInfo(const UpdateInfo& info);
    
    /**
     * @brief Check if an update is compatible with the current hardware and firmware
     * @param info The UpdateInfo to evaluate
     * @param currentFirmware The currently running firmware information
     * @return UpdateError with error status (success if compatible)
     * 
     * Validates:
     * - Hardware variant matches current device
     * - Minimum compatible version requirement is met
     * - Protocol version compatibility
     */
    static UpdateError checkCompatibility(const UpdateInfo& info, const FirmwareInfo& currentFirmware);
    
    /**
     * @brief Determine if an update represents a newer version than current firmware
     * @param info The UpdateInfo to evaluate
     * @param currentFirmware The currently running firmware information
     * @return VersionComparison result (Newer, Equal, or Older)
     */
    static VersionComparison compareVersion(const UpdateInfo& info, const FirmwareInfo& currentFirmware);
    
    /**
     * @brief Validate SHA256 hash format
     * @param sha256 Hex string to validate
     * @return true if string is a valid 64-character hex string
     */
    static bool isValidSha256(const std::string& sha256);
    
    /**
     * @brief Validate URL format (basic scheme check)
     * @param url URL string to validate
     * @return true if URL has a supported scheme (http:// or https://)
     */
    static bool isValidUrl(const std::string& url);
    
    /**
     * @brief Parse a semantic version string to numeric version code
     * @param versionString Semantic version (e.g., "3.1.4")
     * @return Numeric version code (e.g., 30104 for 3.1.4), or 0 on failure
     */
    static uint32_t parseVersionString(const std::string& versionString);
    
    /**
     * @brief Check if minimum version requirement is satisfied
     * @param currentVersion Current firmware numeric version
     * @param minRequired Minimum required version string
     * @return true if currentVersion meets or exceeds the minimum requirement
     */
    static bool meetsMinimumVersion(uint32_t currentVersion, const std::string& minRequired);

private:
    // Private constructor - all methods static, no instantiation needed
    UpdateValidator() = delete;
    ~UpdateValidator() = delete;
};