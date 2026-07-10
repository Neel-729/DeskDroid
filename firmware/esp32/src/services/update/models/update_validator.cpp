#include "update_validator.h"
#include <cctype>
#include <sstream>
#include <vector>
#include <string>

UpdateError UpdateValidator::validateUpdateInfo(const UpdateInfo& info) {
    // Check required basic fields
    if (info.version.empty()) {
        return UpdateError::create(UpdateErrorCode::ValidationInvalidInput, "Version string cannot be empty");
    }
    
    if (info.numericVersion == 0) {
        return UpdateError::create(UpdateErrorCode::ValidationInvalidInput, "Numeric version must be greater than 0");
    }
    
    if (info.downloadUrl.empty()) {
        return UpdateError::create(UpdateErrorCode::ValidationInvalidInput, "Download URL cannot be empty");
    }
    
    if (!isValidUrl(info.downloadUrl)) {
        return UpdateError::create(UpdateErrorCode::ValidationInvalidFormat, "Invalid URL scheme (must be http:// or https://)");
    }
    
    if (info.fileSize == 0) {
        return UpdateError::create(UpdateErrorCode::ValidationInvalidInput, "File size must be greater than 0");
    }
    
    if (info.sha256.empty()) {
        return UpdateError::create(UpdateErrorCode::ValidationInvalidInput, "SHA256 hash cannot be empty");
    }
    
    if (!isValidSha256(info.sha256)) {
        return UpdateError::create(UpdateErrorCode::ValidationInvalidFormat, "SHA256 hash must be 64 hex characters");
    }
    
    // Validate version string can be parsed
    uint32_t parsedVersion = parseVersionString(info.version);
    if (parsedVersion == 0) {
        return UpdateError::create(UpdateErrorCode::VersionInvalidFormat, "Failed to parse version string");
    }
    
    if (parsedVersion != info.numericVersion) {
        return UpdateError::create(UpdateErrorCode::VersionParseFailed, "Numeric version mismatch with version string");
    }
    
    // If min compatible version is provided, validate it can be parsed
    if (!info.minCompatibleVersion.empty()) {
        uint32_t parsedMinVersion = parseVersionString(info.minCompatibleVersion);
        if (parsedMinVersion == 0) {
            return UpdateError::create(UpdateErrorCode::VersionInvalidFormat, "Failed to parse minimum compatible version");
        }
    }
    
    return UpdateError::ok();
}

UpdateError UpdateValidator::checkCompatibility(const UpdateInfo& info, const FirmwareInfo& currentFirmware) {
    // Check hardware variant compatibility if both are specified
    if (!info.hardwareVariant.empty() && currentFirmware.hardwareRevision != nullptr) {
        std::string currentHardware(currentFirmware.hardwareRevision);
        if (info.hardwareVariant != currentHardware) {
            return UpdateError::create(
                UpdateErrorCode::CompatibilityHardwareMismatch,
                "Hardware variant mismatch: update requires '" + info.hardwareVariant + 
                "', current hardware is '" + currentHardware + "'"
            );
        }
    }
    
    // Check minimum version requirement if specified
    if (!info.minCompatibleVersion.empty()) {
        if (!meetsMinimumVersion(currentFirmware.versionCode, info.minCompatibleVersion)) {
            uint32_t minRequired = parseVersionString(info.minCompatibleVersion);
            return UpdateError::create(
                UpdateErrorCode::CompatibilityMinimumVersionNotMet,
                "Current firmware version " + std::string(currentFirmware.firmwareVersion) +
                " does not meet minimum required version " + info.minCompatibleVersion +
                " (code " + std::to_string(minRequired) + " required, " + 
                std::to_string(currentFirmware.versionCode) + " current)"
            );
        }
    }
    
    // All compatibility checks passed
    return UpdateError::ok();
}

VersionComparison UpdateValidator::compareVersion(const UpdateInfo& info, const FirmwareInfo& currentFirmware) {
    return VersionCompare::compare(currentFirmware.versionCode, info.numericVersion);
}

bool UpdateValidator::isValidSha256(const std::string& sha256) {
    if (sha256.length() != 64) {
        return false;
    }
    
    // Verify all characters are hexadecimal
    for (char c : sha256) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    
    return true;
}

bool UpdateValidator::isValidUrl(const std::string& url) {
    // Basic scheme validation - must be http or https
    if (url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://") {
        return true;
    }
    return false;
}

uint32_t UpdateValidator::parseVersionString(const std::string& versionString) {
    if (versionString.empty()) {
        return 0;
    }
    
    std::vector<int> parts;
    std::stringstream ss(versionString);
    std::string part;
    
    // Split version string by dots
    while (std::getline(ss, part, '.')) {
        try {
            int num = std::stoi(part);
            if (num < 0 || num > 255) { // Each component max 255 to fit in 8 bits
                return 0;
            }
            parts.push_back(num);
        } catch (...) {
            return 0; // Failed to parse number
        }
    }
    
    // Need at least major.minor.patch (3 components)
    if (parts.size() < 3) {
        return 0;
    }
    
    // Encode as [major(8) . minor(8) . patch(16)] to match firmware_version.h convention
    // This matches the 2.6.8 -> 20608 calculation used in the current version
    uint32_t versionCode = (parts[0] * 10000) + (parts[1] * 100) + parts[2];
    
    return versionCode;
}

bool UpdateValidator::meetsMinimumVersion(uint32_t currentVersion, const std::string& minRequired) {
    uint32_t minVersion = parseVersionString(minRequired);
    if (minVersion == 0) {
        return false;
    }
    
    // Current version must be >= minimum required version
    return currentVersion >= minVersion;
}