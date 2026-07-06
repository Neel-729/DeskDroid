#pragma once

#include "update_provider.h"
#include "../models/update_info.h"
#include "../models/update_decision.h"

class NullUpdateProvider : public IUpdateProvider {
public:
    bool begin() override;
    void loop() override;
    bool available() const override;
    const char* providerName() const override;
    
    /**
     * @brief Get the update decision from the null provider
     * @return UpdateDecisionContext indicating no update is available
     * 
     * Returns a realistic NoUpdateAvailable decision with properly
     * formatted empty/dummy UpdateInfo that passes basic validation checks.
     */
    UpdateDecisionContext getUpdateDecision() const;
    
    /**
     * @brief Get the dummy update info maintained by the null provider
     * @return const UpdateInfo& Empty but properly structured update info
     */
    const UpdateInfo& getDummyUpdateInfo() const;

private:
    UpdateInfo m_dummyUpdateInfo; ///< Dummy update info for realistic API compliance
};