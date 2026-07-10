#pragma once

#include <Arduino.h>
#include "boot_validation_types.h"
#include "iboot_validation_policy.h"

namespace BootValidation {

class BootValidationManager {
public:
    BootValidationManager();

    bool initialize(const IBootValidationPolicy* policy = nullptr);
    bool beginValidation(const IBootValidationPolicy* policy = nullptr);
    void update(const BootHealth& health);
    const BootValidationReport& report() const;
    BootState state() const;
    bool isHealthy() const;
    void reset();

private:
    void transitionTo(BootState newState);
    void clearContext();
    void clearHealth();
    void initializeReport();
    bool allRequiredHealthChecksPassed(const BootHealth& health) const;
    bool anyRequiredHealthCheckFailed(const BootHealth& health) const;
    bool detectFirstBootAfterOTA(const esp_partition_t* runningPartition,
                                 const esp_partition_t* bootPartition,
                                 bool* otaPendingValidation);

#ifdef OTA_PLATFORM_VALIDATION
    void logHealthChecks(const BootHealth& health) const;
    void validateInvariants(BootState previousState) const;
#endif

    BootState _currentState;
    BootState _previousState;
    BootContext _context;
    BootHealth _health;
    BootValidationReport _report;
    const IBootValidationPolicy* _policy;
    bool _initialized;
};

} // namespace BootValidation