#include "null_provider.h"
#include "../../../core/logging.h"

bool NullUpdateProvider::begin() {
    // Initialize dummy update info with realistic but empty values
    // This ensures the structure is properly populated for API compliance
    m_dummyUpdateInfo.version = "0.0.0";
    m_dummyUpdateInfo.numericVersion = 0;
    m_dummyUpdateInfo.downloadUrl = "";
    m_dummyUpdateInfo.fileSize = 0;
    m_dummyUpdateInfo.sha256 = std::string(64, '0'); // Valid 64-character hex string
    m_dummyUpdateInfo.releaseNotes = "No updates available from null provider";
    m_dummyUpdateInfo.releaseDate = "";
    m_dummyUpdateInfo.minCompatibleVersion = "0.0.0";
    m_dummyUpdateInfo.hardwareVariant = "generic";
    
    LOG_INFO(LogTag::UPDATE, "Null provider initialized with dummy update information");
    return true;
}

void NullUpdateProvider::loop() {
    // No-op - no work to perform, remains completely passive
}

bool NullUpdateProvider::available() const {
    return false;
}

const char* NullUpdateProvider::providerName() const {
    return "None";
}

UpdateDecisionContext NullUpdateProvider::getUpdateDecision() const {
    // Return a realistic NoUpdateAvailable decision with proper context
    return UpdateDecisionContext::create(
        UpdateDecisionType::NoUpdateAvailable,
        "Null provider always reports no updates available"
    );
}

const UpdateInfo& NullUpdateProvider::getDummyUpdateInfo() const {
    return m_dummyUpdateInfo;
}