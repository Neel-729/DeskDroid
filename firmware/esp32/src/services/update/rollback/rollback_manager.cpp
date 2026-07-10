#include "rollback_manager.h"

namespace Rollback {

RollbackManager::RollbackManager()
    : _currentState(RollbackState::Idle)
    , _previousState(RollbackState::Idle)
    , _initialized(false)
    , _reportInitialized(false)
    , _evaluationAttempted(false)
    , _executionAttempted(false)
    , _espApiFailed(false)
    , _policyApproved(false)
    , _rollbackCallCount(0)
    , _runningPartition(nullptr)
    , _bootPartition(nullptr) {
    clearContext();
    clearEspDiagnostics();
    initializeReport();
    _reportInitialized = false;
}

bool RollbackManager::initialize(const RollbackContext& context) {
    if (_currentState != RollbackState::Idle) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[Rollback] initialize() rejected from state=%s\n",
               rollbackStateToString(_currentState));
#endif
        return false;
    }

    _context = context;
    _initialized = true;
    _evaluationAttempted = false;
    _executionAttempted = false;
    _espApiFailed = false;
    _policyApproved = false;
    _rollbackCallCount = 0;
    clearEspDiagnostics();
    initializeReport();
    _reportInitialized = true;
    transitionTo(RollbackState::Idle);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Rollback] Initialized bootContext=%p bootReport=%p acceptanceReport=%p policy=%p\n",
           reinterpret_cast<const void*>(_context.bootContext),
           reinterpret_cast<const void*>(_context.bootReport),
           reinterpret_cast<const void*>(_context.acceptanceReport),
           reinterpret_cast<const void*>(_context.policy));
#endif

    return true;
}

RollbackReport RollbackManager::evaluate() {
    if (isTerminalState(_currentState)) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[Rollback] evaluate() ignored in terminal state=%s\n",
               rollbackStateToString(_currentState));
#endif
        return _report;
    }

    if (_currentState == RollbackState::ReadyForRollback) {
        return _report;
    }

    transitionTo(RollbackState::Evaluating);
    _evaluationAttempted = true;
    collectEspState();

    RollbackReason reason = determineRollbackReason();
    bool rollbackRequired = reason == RollbackReason::BootValidationFailed ||
                            reason == RollbackReason::AcceptanceFailed;
    bool policyApproved = isPolicyApproved(reason);
    _policyApproved = policyApproved;

#ifdef OTA_PLATFORM_VALIDATION
    logDiagnostics("evaluate");
#endif

    if (!_initialized) {
        transitionTo(RollbackState::Rejected);
        setReport(RollbackResult::Rejected, _currentState, RollbackReason::Unknown,
                  _rollbackPossibleBefore, false, false, false, false,
                  "Rollback manager not initialized");
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return _report;
    }

    if (_runningPartition == nullptr || _bootPartition == nullptr) {
        transitionTo(RollbackState::Rejected);
        setReport(RollbackResult::Rejected, _currentState, RollbackReason::ImageInvalid,
                  _rollbackPossibleBefore, false, false, false, false,
                  "Running or boot partition missing");
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return _report;
    }

    if (!rollbackRequired) {
        transitionTo(RollbackState::Rejected);
        setReport(RollbackResult::Rejected, _currentState, RollbackReason::None,
                  _rollbackPossibleBefore, false, false, false, false,
                  "Rollback not required");
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return _report;
    }

    if (!_rollbackPossibleBefore) {
        transitionTo(RollbackState::Rejected);
        setReport(RollbackResult::Rejected, _currentState, RollbackReason::RollbackUnavailable,
                  false, false, false, false, false,
                  "ESP-IDF reports rollback unavailable");
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return _report;
    }

    if (!policyApproved) {
        transitionTo(RollbackState::Rejected);
        setReport(RollbackResult::Rejected, _currentState, RollbackReason::PolicyDenied,
                  _rollbackPossibleBefore, false, false, false, false,
                  _context.policy != nullptr && _context.policy->reason() != nullptr
                      ? _context.policy->reason()
                      : "Rollback denied by policy");
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return _report;
    }

    transitionTo(RollbackState::ReadyForRollback);
    setReport(RollbackResult::Ready, _currentState, reason,
              _rollbackPossibleBefore, true, false, false, false,
              "Rollback ready");

#ifdef OTA_PLATFORM_VALIDATION
    validateInvariants(_previousState);
#endif

    return _report;
}

RollbackReport RollbackManager::executeRollback() {
    if (isTerminalState(_currentState)) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[Rollback] executeRollback() ignored in terminal state=%s calls=%u\n",
               rollbackStateToString(_currentState),
               _rollbackCallCount);
#endif
        return _report;
    }

    if (_executionAttempted || _rollbackCallCount > 0) {
        transitionTo(RollbackState::Error);
        setReport(RollbackResult::Failure, _currentState, RollbackReason::ExecutionFailed,
                  _report.rollbackPossible, _report.rollbackRequired, _report.rollbackExecuted,
                  _report.rebootTriggered, false,
                  "Rollback execution already attempted");
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return _report;
    }

    if (_currentState == RollbackState::Idle || _currentState == RollbackState::Evaluating) {
        evaluate();
        if (isTerminalState(_currentState)) {
            return _report;
        }
    }

    RollbackReason rejectedReason = RollbackReason::Unknown;
    const char* rejectedMessage = "";
    if (!preconditionsMet(&rejectedReason, &rejectedMessage)) {
        transitionTo(RollbackState::Rejected);
        setReport(RollbackResult::Rejected, _currentState, rejectedReason,
                  _report.rollbackPossible, false, false, false, false,
                  rejectedMessage);
#ifdef OTA_PLATFORM_VALIDATION
        logDiagnostics("precondition-rejected");
        validateInvariants(_previousState);
#endif
        return _report;
    }

    transitionTo(RollbackState::Executing);
    _executionAttempted = true;
    _runningPartition = esp_ota_get_running_partition();
    _bootPartition = esp_ota_get_boot_partition();
    _stateReadImmediateResult = ESP_FAIL;
    _otaStateImmediate = ESP_OTA_IMG_UNDEFINED;
    if (_runningPartition != nullptr) {
        _stateReadImmediateResult = esp_ota_get_state_partition(_runningPartition, &_otaStateImmediate);
    }
    _rollbackPossibleImmediate = esp_ota_check_rollback_is_possible();

#ifdef OTA_PLATFORM_VALIDATION
    logDiagnostics("before-execute");
#endif

    if (_runningPartition == nullptr ||
        _bootPartition == nullptr ||
        !_rollbackPossibleImmediate ||
        _stateReadImmediateResult != ESP_OK ||
        !otaImageStateSupportsRollback(_otaStateImmediate)) {
        transitionTo(RollbackState::Rejected);
        setReport(RollbackResult::Rejected, _currentState,
                  !_rollbackPossibleImmediate ? RollbackReason::RollbackUnavailable : RollbackReason::ImageInvalid,
                  _rollbackPossibleImmediate, false, false, false, false,
                  !_rollbackPossibleImmediate
                      ? "Rollback unavailable immediately before execution"
                      : "OTA image state changed before rollback");
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return _report;
    }

    _rollbackCallCount++;

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Rollback] Calling esp_ota_mark_app_invalid_rollback_and_reboot()\n");
#endif

    _rollbackResult = esp_ota_mark_app_invalid_rollback_and_reboot();

    _stateReadAfterResult = esp_ota_get_state_partition(_runningPartition, &_otaStateAfter);
    _rollbackPossibleAfter = esp_ota_check_rollback_is_possible();

#ifdef OTA_PLATFORM_VALIDATION
    logDiagnostics("after-execute-returned");
#endif

    _espApiFailed = true;
    transitionTo(RollbackState::Error);
    setReport(RollbackResult::Failure, _currentState, RollbackReason::ExecutionFailed,
              _rollbackPossibleImmediate, true, true, false, false,
              _rollbackResult == ESP_OK
                  ? "Rollback API returned unexpectedly after reboot request"
                  : "ESP-IDF rollback execution failed");

#ifdef OTA_PLATFORM_VALIDATION
    validateInvariants(_previousState);
#endif

    return _report;
}

const RollbackReport& RollbackManager::report() const {
    return _report;
}

RollbackState RollbackManager::state() const {
    return _currentState;
}

void RollbackManager::reset() {
#ifdef OTA_PLATFORM_VALIDATION
    printf("[Rollback] Reset requested from state=%s\n",
           rollbackStateToString(_currentState));
#endif

    clearContext();
    clearEspDiagnostics();
    _initialized = false;
    _evaluationAttempted = false;
    _executionAttempted = false;
    _espApiFailed = false;
    _policyApproved = false;
    _rollbackCallCount = 0;
    initializeReport();
    _reportInitialized = false;
    _previousState = _currentState;
    _currentState = RollbackState::Idle;
}

void RollbackManager::transitionTo(RollbackState newState) {
    if (!isLegalTransition(_currentState, newState)) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[ROLLBACK][INVARIANT] Illegal transition rejected: %s -> %s\n",
               rollbackStateToString(_currentState),
               rollbackStateToString(newState));
#endif
        return;
    }

    if (_currentState == newState) {
        return;
    }

    const RollbackState previous = _currentState;

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Rollback] State transition: %s -> %s\n",
           rollbackStateToString(previous),
           rollbackStateToString(newState));
#endif

    _previousState = previous;
    _currentState = newState;
}

bool RollbackManager::isTerminalState(RollbackState state) const {
    return state == RollbackState::Completed ||
           state == RollbackState::Rejected ||
           state == RollbackState::Error;
}

bool RollbackManager::isLegalTransition(RollbackState from, RollbackState to) const {
    if (from == to) {
        return true;
    }

    if (isTerminalState(from)) {
        return false;
    }

    if (from == RollbackState::Idle && to == RollbackState::Evaluating) {
        return true;
    }

    if (from == RollbackState::Evaluating &&
        (to == RollbackState::ReadyForRollback || to == RollbackState::Rejected)) {
        return true;
    }

    if (from == RollbackState::ReadyForRollback &&
        (to == RollbackState::Executing || to == RollbackState::Rejected)) {
        return true;
    }

    if (from == RollbackState::Executing &&
        (to == RollbackState::Completed ||
         to == RollbackState::Error ||
         to == RollbackState::Rejected)) {
        return true;
    }

    return false;
}

void RollbackManager::clearContext() {
    _context.bootContext = nullptr;
    _context.bootReport = nullptr;
    _context.acceptanceReport = nullptr;
    _context.policy = nullptr;
}

void RollbackManager::clearEspDiagnostics() {
    _runningPartition = nullptr;
    _bootPartition = nullptr;
    _stateReadBeforeResult = ESP_FAIL;
    _stateReadImmediateResult = ESP_FAIL;
    _stateReadAfterResult = ESP_FAIL;
    _rollbackResult = ESP_FAIL;
    _otaStateBefore = ESP_OTA_IMG_UNDEFINED;
    _otaStateImmediate = ESP_OTA_IMG_UNDEFINED;
    _otaStateAfter = ESP_OTA_IMG_UNDEFINED;
    _rollbackPossibleBefore = false;
    _rollbackPossibleImmediate = false;
    _rollbackPossibleAfter = false;
}

void RollbackManager::initializeReport() {
    _report.result = RollbackResult::Unknown;
    _report.state = RollbackState::Idle;
    _report.reason = RollbackReason::None;
    _report.rollbackPossible = false;
    _report.rollbackRequired = false;
    _report.rollbackExecuted = false;
    _report.rebootTriggered = false;
    _report.rollbackSuccessful = false;
    _report.timestamp = 0;
    _report.message = "";
}

void RollbackManager::setReport(RollbackResult result,
                                RollbackState state,
                                RollbackReason reason,
                                bool rollbackPossible,
                                bool rollbackRequired,
                                bool rollbackExecuted,
                                bool rebootTriggered,
                                bool rollbackSuccessful,
                                const char* message) {
    _report.result = result;
    _report.state = state;
    _report.reason = reason;
    _report.rollbackPossible = rollbackPossible;
    _report.rollbackRequired = rollbackRequired;
    _report.rollbackExecuted = rollbackExecuted;
    _report.rebootTriggered = rebootTriggered;
    _report.rollbackSuccessful = rollbackSuccessful;
    _report.timestamp = millis();
    _report.message = message != nullptr ? message : "";

#ifdef OTA_PLATFORM_VALIDATION
    logReport();
#endif
}

void RollbackManager::collectEspState() {
    _runningPartition = esp_ota_get_running_partition();
    _bootPartition = esp_ota_get_boot_partition();
    _rollbackPossibleBefore = esp_ota_check_rollback_is_possible();
    _stateReadBeforeResult = ESP_FAIL;
    _otaStateBefore = ESP_OTA_IMG_UNDEFINED;

    if (_runningPartition != nullptr) {
        _stateReadBeforeResult = esp_ota_get_state_partition(_runningPartition, &_otaStateBefore);
    }
}

RollbackReason RollbackManager::determineRollbackReason() const {
    if (_context.bootReport != nullptr &&
        (_context.bootReport->result == BootValidation::BootValidationResult::Failed ||
         _context.bootReport->state == BootValidation::BootState::Unhealthy)) {
        return RollbackReason::BootValidationFailed;
    }

    if (_context.acceptanceReport != nullptr &&
        (_context.acceptanceReport->result == Acceptance::AcceptanceResult::Failure ||
         _context.acceptanceReport->state == Acceptance::AcceptanceState::Error)) {
        return RollbackReason::AcceptanceFailed;
    }

    return RollbackReason::None;
}

bool RollbackManager::isPolicyApproved(RollbackReason reason) const {
    return _context.policy != nullptr && _context.policy->permitRollback(reason);
}

bool RollbackManager::otaImageStateSupportsRollback(esp_ota_img_states_t state) const {
    return state == ESP_OTA_IMG_PENDING_VERIFY;
}

bool RollbackManager::preconditionsMet(RollbackReason* rejectedReason, const char** rejectedMessage) {
    if (rejectedReason != nullptr) {
        *rejectedReason = RollbackReason::Unknown;
    }
    if (rejectedMessage != nullptr) {
        *rejectedMessage = "";
    }

    collectEspState();

    if (!_initialized) {
        if (rejectedMessage != nullptr) {
            *rejectedMessage = "Rollback manager not initialized";
        }
        return false;
    }

    if (!_reportInitialized) {
        if (rejectedMessage != nullptr) {
            *rejectedMessage = "Rollback report not initialized";
        }
        return false;
    }

    if (_runningPartition == nullptr) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::ImageInvalid;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = "Running partition missing";
        }
        return false;
    }

    if (_bootPartition == nullptr) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::ImageInvalid;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = "Boot partition missing";
        }
        return false;
    }

    if (!_rollbackPossibleBefore) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::RollbackUnavailable;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = "Rollback unavailable";
        }
        return false;
    }

    if (!_report.rollbackRequired) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::None;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = "Rollback not required";
        }
        return false;
    }

    const RollbackReason reason = determineRollbackReason();
    if (!isPolicyApproved(reason)) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::PolicyDenied;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = _context.policy != nullptr && _context.policy->reason() != nullptr
                                   ? _context.policy->reason()
                                   : "Rollback denied by policy";
        }
        return false;
    }
    _policyApproved = true;

    if (!_context.policy->noActiveInstallerSession()) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::PolicyDenied;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = _context.policy->reason() != nullptr
                                   ? _context.policy->reason()
                                   : "Installer session is active";
        }
        return false;
    }

    if (!_context.policy->noActiveOtaWriteSession()) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::PolicyDenied;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = _context.policy->reason() != nullptr
                                   ? _context.policy->reason()
                                   : "OTA write session is active";
        }
        return false;
    }

    if (!_context.policy->noPendingFirmwareAcceptance()) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::PolicyDenied;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = _context.policy->reason() != nullptr
                                   ? _context.policy->reason()
                                   : "Firmware acceptance is pending";
        }
        return false;
    }

    if (_stateReadBeforeResult != ESP_OK ||
        !otaImageStateSupportsRollback(_otaStateBefore)) {
        if (rejectedReason != nullptr) {
            *rejectedReason = RollbackReason::ImageInvalid;
        }
        if (rejectedMessage != nullptr) {
            *rejectedMessage = "OTA image state does not support rollback";
        }
        return false;
    }

    return true;
}

bool RollbackManager::partitionsMatch(const esp_partition_t* first,
                                      const esp_partition_t* second) const {
    if (first == nullptr || second == nullptr) {
        return false;
    }

    return first == second ||
           (first->address == second->address &&
            first->size == second->size &&
            first->type == second->type &&
            first->subtype == second->subtype);
}

#ifdef OTA_PLATFORM_VALIDATION
void RollbackManager::logDiagnostics(const char* phase) const {
    printf("[Rollback] Diagnostics phase=%s\n", phase != nullptr ? phase : "");
    printf("[Rollback] Running partition: %s (%p)\n",
           _runningPartition != nullptr ? _runningPartition->label : "nullptr",
           reinterpret_cast<const void*>(_runningPartition));
    printf("[Rollback] Boot partition: %s (%p)\n",
           _bootPartition != nullptr ? _bootPartition->label : "nullptr",
           reinterpret_cast<const void*>(_bootPartition));
    printf("[Rollback] OTA image state before result=0x%x state=%d\n",
           _stateReadBeforeResult, static_cast<int>(_otaStateBefore));
    printf("[Rollback] OTA image state immediate result=0x%x state=%d\n",
           _stateReadImmediateResult, static_cast<int>(_otaStateImmediate));
    printf("[Rollback] OTA image state after result=0x%x state=%d\n",
           _stateReadAfterResult, static_cast<int>(_otaStateAfter));
    printf("[Rollback] Rollback possible before=%d immediate=%d after=%d\n",
           _rollbackPossibleBefore,
           _rollbackPossibleImmediate,
           _rollbackPossibleAfter);
    printf("[Rollback] Rollback required=%d policyApproved=%d reason=%s\n",
           _report.rollbackRequired,
           _policyApproved,
           rollbackReasonToString(_report.reason));
    printf("[Rollback] ESP-IDF return value=0x%x callCount=%u\n",
           _rollbackResult,
           _rollbackCallCount);
    printf("[Rollback] Execution timestamp=%lu\n",
           static_cast<unsigned long>(_report.timestamp));
}

void RollbackManager::logReport() const {
    printf("[Rollback] Report result=%s state=%s reason=%s possible=%d required=%d executed=%d reboot=%d success=%d timestamp=%lu message=%s\n",
           rollbackResultToString(_report.result),
           rollbackStateToString(_report.state),
           rollbackReasonToString(_report.reason),
           _report.rollbackPossible,
           _report.rollbackRequired,
           _report.rollbackExecuted,
           _report.rebootTriggered,
           _report.rollbackSuccessful,
           static_cast<unsigned long>(_report.timestamp),
           _report.message != nullptr ? _report.message : "");
}

void RollbackManager::validateInvariants(RollbackState previousState) const {
    if (_report.rollbackExecuted && !_report.rollbackRequired) {
        printf("[ROLLBACK][INVARIANT] rollbackExecuted requires rollbackRequired\n");
    }

    if (_report.rollbackExecuted && !_report.rollbackPossible) {
        printf("[ROLLBACK][INVARIANT] rollbackExecuted requires rollbackPossible\n");
    }

    if (_report.rollbackExecuted && !_policyApproved) {
        printf("[ROLLBACK][INVARIANT] rollbackExecuted requires policy approval\n");
    }

    if (_report.rollbackExecuted && _rollbackCallCount != 1) {
        printf("[ROLLBACK][INVARIANT] rollbackExecuted requires exactly one ESP-IDF call, got %u\n",
               _rollbackCallCount);
    }

    if (_currentState == RollbackState::Completed && !_report.rollbackExecuted) {
        printf("[ROLLBACK][INVARIANT] Completed requires rollbackExecuted == true\n");
    }

    if (_currentState == RollbackState::Rejected && _report.rollbackExecuted) {
        printf("[ROLLBACK][INVARIANT] Rejected requires rollbackExecuted == false\n");
    }

    if (_currentState == RollbackState::Rejected && _rollbackCallCount != 0) {
        printf("[ROLLBACK][INVARIANT] Rejected requires ESP-IDF never called\n");
    }

    if (_currentState == RollbackState::Error &&
        (!_espApiFailed || _rollbackCallCount != 1)) {
        printf("[ROLLBACK][INVARIANT] Error requires execution failure\n");
    }

    if (_currentState == RollbackState::Idle && _report.timestamp != 0) {
        printf("[ROLLBACK][INVARIANT] Idle requires no generated report\n");
    }

    if (_currentState == RollbackState::ReadyForRollback && !_report.rollbackPossible) {
        printf("[ROLLBACK][INVARIANT] ReadyForRollback requires rollbackPossible == true\n");
    }

    if (_currentState == RollbackState::Executing &&
        previousState != RollbackState::ReadyForRollback) {
        printf("[ROLLBACK][INVARIANT] Executing requires previous ReadyForRollback state\n");
    }
}
#endif

} // namespace Rollback
