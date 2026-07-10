#pragma once

#include <Arduino.h>
#include <esp_ota_ops.h>
#include "acceptance_types.h"
#include "iacceptance_policy.h"

namespace Acceptance {

class FirmwareAcceptanceManager {
public:
    FirmwareAcceptanceManager();

    bool initialize(const IAcceptancePolicy* policy = nullptr);
    AcceptanceReport accept(const AcceptanceContext& context);
    const AcceptanceReport& report() const;
    AcceptanceState state() const;
    void reset();

private:
    void transitionTo(AcceptanceState newState);
    bool isLegalTransition(AcceptanceState from, AcceptanceState to) const;
    void clearContext();
    void clearEspDiagnostics();
    void initializeReport();
    void setReport(AcceptanceResult result,
                   AcceptanceState state,
                   const char* message,
                   bool rollbackCancelled,
                   bool firmwarePermanent);
    bool isHealthyCompletedValidation(const BootValidation::BootValidationReport* bootReport) const;
    bool isOtaBootOrigin(const BootValidation::BootContext* bootContext) const;
    bool partitionsMatch(const esp_partition_t* first, const esp_partition_t* second) const;

#ifdef OTA_PLATFORM_VALIDATION
    void validateInvariants(AcceptanceState previousState,
                            const esp_partition_t* runningPartition,
                            const esp_partition_t* bootPartition) const;
    void logPreconditionFailure(const char* message) const;
#endif

    AcceptanceState _currentState;
    AcceptanceState _previousState;
    AcceptanceReport _report;
    AcceptanceContext _context;
    const IAcceptancePolicy* _policy;
    bool _initialized;
    bool _acceptanceAttempted;
    bool _espApiFailed;
    uint8_t _markValidCallCount;
    esp_err_t _stateReadBeforeResult;
    esp_err_t _stateReadImmediateResult;
    esp_err_t _markValidResult;
    esp_err_t _stateReadAfterResult;
    esp_ota_img_states_t _otaStateBefore;
    esp_ota_img_states_t _otaStateImmediate;
    esp_ota_img_states_t _otaStateAfter;
};

} // namespace Acceptance
