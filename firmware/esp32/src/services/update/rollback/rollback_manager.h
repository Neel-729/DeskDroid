#pragma once

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "rollback_types.h"

namespace Rollback {

class RollbackManager {
public:
    RollbackManager();

    bool initialize(const RollbackContext& context);
    RollbackReport evaluate();
    RollbackReport executeRollback();
    const RollbackReport& report() const;
    RollbackState state() const;
    void reset();

private:
    void transitionTo(RollbackState newState);
    bool isTerminalState(RollbackState state) const;
    bool isLegalTransition(RollbackState from, RollbackState to) const;
    void clearContext();
    void clearEspDiagnostics();
    void initializeReport();
    void setReport(RollbackResult result,
                   RollbackState state,
                   RollbackReason reason,
                   bool rollbackPossible,
                   bool rollbackRequired,
                   bool rollbackExecuted,
                   bool rebootTriggered,
                   bool rollbackSuccessful,
                   const char* message);
    void collectEspState();
    RollbackReason determineRollbackReason() const;
    bool isPolicyApproved(RollbackReason reason) const;
    bool otaImageStateSupportsRollback(esp_ota_img_states_t state) const;
    bool preconditionsMet(RollbackReason* rejectedReason, const char** rejectedMessage);
    bool partitionsMatch(const esp_partition_t* first, const esp_partition_t* second) const;

#ifdef OTA_PLATFORM_VALIDATION
    void logDiagnostics(const char* phase) const;
    void logReport() const;
    void validateInvariants(RollbackState previousState) const;
#endif

    RollbackState _currentState;
    RollbackState _previousState;
    RollbackReport _report;
    RollbackContext _context;
    bool _initialized;
    bool _reportInitialized;
    bool _evaluationAttempted;
    bool _executionAttempted;
    bool _espApiFailed;
    bool _policyApproved;
    uint8_t _rollbackCallCount;
    const esp_partition_t* _runningPartition;
    const esp_partition_t* _bootPartition;
    esp_err_t _stateReadBeforeResult;
    esp_err_t _stateReadImmediateResult;
    esp_err_t _stateReadAfterResult;
    esp_err_t _rollbackResult;
    esp_ota_img_states_t _otaStateBefore;
    esp_ota_img_states_t _otaStateImmediate;
    esp_ota_img_states_t _otaStateAfter;
    bool _rollbackPossibleBefore;
    bool _rollbackPossibleImmediate;
    bool _rollbackPossibleAfter;
};

} // namespace Rollback
