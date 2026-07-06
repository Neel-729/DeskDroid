#pragma once

#include <Arduino.h>
#include <string>
#include "../firmware_info.h"
#include "../version_compare.h"

/**
 * @brief Comprehensive information about an available firmware update
 * 
 * Contains all metadata required to evaluate, download, verify, and install
 * a firmware update. Includes version information, download location, security
 * hashes, release details, and compatibility constraints.
 */
struct UpdateInfo {
    /// Semantic version string (e.g., "3.0.1")
    std::string version;
    
    /// Numeric version code for efficient comparison (e.g., 30001 for 3.0.1)
    uint32_t numericVersion = 0;
    
    /// Complete URL to download the firmware binary
    std::string downloadUrl;
    
    /// Total size of the firmware binary in bytes
    size_t fileSize = 0;
    
    /// SHA-256 hash of the firmware binary as a hex string (64 characters)
    std::string sha256;
    
    /// Markdown or plain text release notes describing changes
    std::string releaseNotes;
    
    /// ISO 8601 formatted release date (e.g., "2024-01-15")
    std::string releaseDate;
    
    /// Minimum device firmware version compatible with this update
    std::string minCompatibleVersion;
    
    /// Target hardware variant this firmware is intended for
    std::string hardwareVariant;
    
    /**
     * @brief Perform basic validation of required fields
     * @return true if all mandatory fields are present and valid
     * 
     * Validates that version, numericVersion, downloadUrl, fileSize, and sha256
     * are properly populated. This is a minimal sanity check to catch missing
     * critical information before further processing.
     */
    bool isValid() const {
        return !version.empty() && 
               numericVersion > 0 && 
               !downloadUrl.empty() && 
               fileSize > 0 && 
               sha256.length() == 64;
    }
    
    /**
     * @brief Compare if this update is newer than the currently running firmware
     * @param current The FirmwareInfo of the currently running firmware
     * @return true if this update has a higher version code than current firmware
     * 
     * Uses the existing VersionCompare utility to safely compare version codes,
     * ensuring consistent version evaluation across the entire OTA system.
     */
    bool isNewerThanCurrent(const FirmwareInfo& current) const {
        VersionComparison comparison = VersionCompare::compare(current.versionCode, numericVersion);
        return comparison == VersionComparison::Newer;
    }
    
    /**
     * @brief Default constructor creates an empty, invalid UpdateInfo
     */
    UpdateInfo() = default;
    
    /**
     * @brief Copy constructor
     */
    UpdateInfo(const UpdateInfo&) = default;
    
    /**
     * @brief Copy assignment
     */
    UpdateInfo& operator=(const UpdateInfo&) = default;
};