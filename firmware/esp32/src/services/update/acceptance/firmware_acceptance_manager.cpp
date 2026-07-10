#include "firmware_acceptance_manager.h"

namespace Acceptance {

FirmwareAcceptanceManager::FirmwareAcceptanceManager()
    : _currentState(AcceptanceState::Idle)
    , _previousState(AcceptanceState::Idle)
    , _policy(nullptr)
    , _initialized(false)
    , _acceptanceAttempted(false)
    , _espApiFailed(false)
    , _markValidCallCount(0) {
    clearContext();
    clearEspDiagnostics();
    initializeReport();
}

bool FirmwareAcceptanceManager::initialize(const IAcceptancePolicy* policy) {
    if (_currentState != AcceptanceState::Idle) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[Acceptance] initialize() rejected from terminal/non-idle state=%s\n",
               acceptanceStateToString(_currentState));
#endif
        return false;
    }

    _policy = policy;
    _initialized = true;
    _acceptanceAttempted = false;
    _espApiFailed = false;
    _markValidCallCount = 0;
    clearContext();
    clearEspDiagnostics();
    initializeReport();
    transitionTo(AcceptanceState::Idle);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] Initialized policy=%p\n", reinterpret_cast<const void*>(_policy));
#endif

    return true;
}

AcceptanceReport FirmwareAcceptanceManager::accept(const AcceptanceContext& context) {
    if (_acceptanceAttempted) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[Acceptance] accept() idempotent return state=%s result=%s calls=%u timestamp=%llu\n",
               acceptanceStateToString(_currentState),
               acceptanceResultToString(_report.result),
               _markValidCallCount,
               static_cast<unsigned long long>(_report.timestampMs));
#endif
        return _report;
    }

    _acceptanceAttempted = true;
    _context = context;

    const esp_partition_t* runningPartition = esp_ota_get_running_partition();
    const esp_partition_t* bootPartition = esp_ota_get_boot_partition();

    if (runningPartition != nullptr) {
        _stateReadBeforeResult = esp_ota_get_state_partition(runningPartition, &_otaStateBefore);
    }

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] Request timestamp=%llu initialized=%d attempted=%d\n",
           static_cast<unsigned long long>(millis()), _initialized, _acceptanceAttempted);
    printf("[Acceptance] Running partition: %s (%p)\n",
           runningPartition != nullptr ? runningPartition->label : "nullptr",
           reinterpret_cast<const void*>(runningPartition));
    printf("[Acceptance] Boot partition: %s (%p)\n",
           bootPartition != nullptr ? bootPartition->label : "nullptr",
           reinterpret_cast<const void*>(bootPartition));
    printf("[Acceptance] BootOrigin=%s\n",
           context.bootContext != nullptr
               ? BootValidation::bootOriginToString(context.bootContext->bootOrigin)
               : "nullptr");
    printf("[Acceptance] Boot timestamp informational=%llu\n",
           context.bootContext != nullptr
               ? static_cast<unsigned long long>(context.bootContext->bootTimestampMs)
               : 0ULL);
    printf("[Acceptance] Validation result=%s eligibleForMarkValid=%d\n",
           context.bootReport != nullptr
               ? BootValidation::bootValidationResultToString(context.bootReport->result)
               : "nullptr",
           context.bootReport != nullptr ? context.bootReport->eligibleForMarkValid : 0);
    printf("[Acceptance] OTA image state before result=0x%x state=%d\n",
           _stateReadBeforeResult, static_cast<int>(_otaStateBefore));
#endif

    if (_currentState != AcceptanceState::Idle) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[Acceptance] accept() rejected from non-idle state=%s\n",
               acceptanceStateToString(_currentState));
#endif
        return _report;
    }

    transitionTo(AcceptanceState::Pending);

    if (!_initialized) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Acceptance manager not initialized", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (context.bootReport == nullptr) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Boot validation report missing", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (context.bootContext == nullptr) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Boot context missing", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (!isHealthyCompletedValidation(context.bootReport)) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Boot validation has not passed", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (!context.bootReport->eligibleForMarkValid) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Firmware is not eligible for mark-valid", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (!isOtaBootOrigin(context.bootContext)) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Boot origin is not OTA", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (_stateReadBeforeResult != ESP_OK || _otaStateBefore != ESP_OTA_IMG_PENDING_VERIFY) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "OTA image is not pending validation", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (runningPartition == nullptr) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Running partition missing", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (bootPartition == nullptr) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Boot partition missing", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (!partitionsMatch(runningPartition, bootPartition)) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Running partition does not match boot partition", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (!partitionsMatch(runningPartition, context.bootContext->runningPartition) ||
        !partitionsMatch(bootPartition, context.bootContext->bootPartition)) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Boot context partitions are stale", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (_policy == nullptr) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "Acceptance policy missing", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    const bool noActiveInstallerSession = _policy->noActiveInstallerSession();
    const bool noPendingRebootExecution = _policy->noPendingRebootExecution();
    const char* policyReason = _policy->reason();

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] Acceptance policy installerIdle=%d rebootIdle=%d reason=%s\n",
           noActiveInstallerSession, noPendingRebootExecution,
           policyReason != nullptr ? policyReason : "");
#endif

    if (!noActiveInstallerSession) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  policyReason != nullptr ? policyReason : "Installer session is active", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    if (!noPendingRebootExecution) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  policyReason != nullptr ? policyReason : "Reboot execution is pending", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    _stateReadImmediateResult = esp_ota_get_state_partition(runningPartition, &_otaStateImmediate);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] OTA image state immediate result=0x%x state=%d\n",
           _stateReadImmediateResult, static_cast<int>(_otaStateImmediate));
#endif

    if (_stateReadImmediateResult != ESP_OK ||
        _otaStateImmediate != ESP_OTA_IMG_PENDING_VERIFY) {
        transitionTo(AcceptanceState::Rejected);
        setReport(AcceptanceResult::Skipped, _currentState,
                  "OTA image state changed before acceptance", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        logPreconditionFailure(_report.message);
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    transitionTo(AcceptanceState::Accepting);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] Calling esp_ota_mark_app_valid_cancel_rollback()\n");
#endif

    _markValidCallCount++;
    _markValidResult = esp_ota_mark_app_valid_cancel_rollback();

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] esp_ota_mark_app_valid_cancel_rollback() result=0x%x\n",
           _markValidResult);
#endif

    if (_markValidResult != ESP_OK) {
        _espApiFailed = true;
        transitionTo(AcceptanceState::Error);
        setReport(AcceptanceResult::Failure, _currentState,
                  "ESP-IDF firmware acceptance failed", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    _stateReadAfterResult = esp_ota_get_state_partition(runningPartition, &_otaStateAfter);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] OTA image state after result=0x%x state=%d\n",
           _stateReadAfterResult, static_cast<int>(_otaStateAfter));
#endif

    if (_stateReadAfterResult != ESP_OK || _otaStateAfter != ESP_OTA_IMG_VALID) {
        _espApiFailed = true;
        transitionTo(AcceptanceState::Error);
        setReport(AcceptanceResult::Failure, _currentState,
                  "Firmware acceptance post-check failed", false, false);
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState, runningPartition, bootPartition);
#endif
        return _report;
    }

    transitionTo(AcceptanceState::Accepted);
    setReport(AcceptanceResult::Success, _currentState,
              "Firmware accepted and rollback cancelled", true, true);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] Result=%s state=%s rollbackCancelled=%d firmwarePermanent=%d timestamp=%llu\n",
           acceptanceResultToString(_report.result),
           acceptanceStateToString(_report.state),
           _report.rollbackCancelled,
           _report.firmwarePermanent,
           static_cast<unsigned long long>(_report.timestampMs));
    printf("[Acceptance] Preserved ESP-IDF status before=0x%x immediate=0x%x markValid=0x%x after=0x%x\n",
           _stateReadBeforeResult,
           _stateReadImmediateResult,
           _markValidResult,
           _stateReadAfterResult);
    validateInvariants(_previousState, runningPartition, bootPartition);
#endif

    return _report;
}

const AcceptanceReport& FirmwareAcceptanceManager::report() const {
    return _report;
}

AcceptanceState FirmwareAcceptanceManager::state() const {
    return _currentState;
}

void FirmwareAcceptanceManager::reset() {
#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] Reset requested from state=%s\n",
           acceptanceStateToString(_currentState));
#endif

    _policy = nullptr;
    _initialized = false;
    _acceptanceAttempted = false;
    _espApiFailed = false;
    _markValidCallCount = 0;
    clearContext();
    clearEspDiagnostics();
    initializeReport();

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] State transition: %s -> %s\n",
           acceptanceStateToString(_currentState),
           acceptanceStateToString(AcceptanceState::Idle));
#endif

    _previousState = _currentState;
    _currentState = AcceptanceState::Idle;
}

void FirmwareAcceptanceManager::transitionTo(AcceptanceState newState) {
    if (!isLegalTransition(_currentState, newState)) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[ACCEPTANCE][INVARIANT] Illegal transition rejected: %s -> %s\n",
               acceptanceStateToString(_currentState),
               acceptanceStateToString(newState));
#endif
        return;
    }

    if (_currentState == newState) {
        return;
    }

    const AcceptanceState previous = _currentState;

#ifdef OTA_PLATFORM_VALIDATION
    printf("[Acceptance] State transition: %s -> %s\n",
           acceptanceStateToString(previous), acceptanceStateToString(newState));
#endif

    _previousState = previous;
    _currentState = newState;
}

bool FirmwareAcceptanceManager::isLegalTransition(
    AcceptanceState from,
    AcceptanceState to) const {
    if (from == to) {
        return true;
    }

    if (from == AcceptanceState::Idle && to == AcceptanceState::Pending) {
        return true;
    }

    if (from == AcceptanceState::Pending &&
        (to == AcceptanceState::Accepting || to == AcceptanceState::Rejected)) {
        return true;
    }

    if (from == AcceptanceState::Accepting &&
        (to == AcceptanceState::Accepted || to == AcceptanceState::Error)) {
        return true;
    }

    return false;
}

void FirmwareAcceptanceManager::clearContext() {
    _context.bootReport = nullptr;
    _context.bootContext = nullptr;
    _context.bootPolicy = nullptr;
}

void FirmwareAcceptanceManager::clearEspDiagnostics() {
    _stateReadBeforeResult = ESP_FAIL;
    _stateReadImmediateResult = ESP_FAIL;
    _markValidResult = ESP_FAIL;
    _stateReadAfterResult = ESP_FAIL;
    _otaStateBefore = ESP_OTA_IMG_UNDEFINED;
    _otaStateImmediate = ESP_OTA_IMG_UNDEFINED;
    _otaStateAfter = ESP_OTA_IMG_UNDEFINED;
}

void FirmwareAcceptanceManager::initializeReport() {
    _report.result = AcceptanceResult::Unknown;
    _report.state = AcceptanceState::Idle;
    _report.timestampMs = 0;
    _report.message = "";
    _report.rollbackCancelled = false;
    _report.firmwarePermanent = false;
}

void FirmwareAcceptanceManager::setReport(AcceptanceResult result,
                                          AcceptanceState state,
                                          const char* message,
                                          bool rollbackCancelled,
                                          bool firmwarePermanent) {
    _report.result = result;
    _report.state = state;
    _report.timestampMs = millis();
    _report.message = message != nullptr ? message : "";
    _report.rollbackCancelled = rollbackCancelled;
    _report.firmwarePermanent = firmwarePermanent;
}

bool FirmwareAcceptanceManager::isHealthyCompletedValidation(
    const BootValidation::BootValidationReport* bootReport) const {
    return bootReport != nullptr &&
           bootReport->state == BootValidation::BootState::ValidationComplete &&
           bootReport->result == BootValidation::BootValidationResult::Passed;
}

bool FirmwareAcceptanceManager::isOtaBootOrigin(
    const BootValidation::BootContext* bootContext) const {
    return bootContext != nullptr &&
           bootContext->firstBoot &&
           bootContext->otaPendingValidation &&
           bootContext->bootOrigin == BootValidation::BootOrigin::OTA;
}

bool FirmwareAcceptanceManager::partitionsMatch(
    const esp_partition_t* first,
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
void FirmwareAcceptanceManager::validateInvariants(
    AcceptanceState previousState,
    const esp_partition_t* runningPartition,
    const esp_partition_t* bootPartition) const {
    if (_markValidCallCount > 1) {
        printf("[ACCEPTANCE][INVARIANT] Mark-valid API may only be called once\n");
    }

    if (_currentState == AcceptanceState::Accepting ||
        _currentState == AcceptanceState::Accepted) {
        if (runningPartition == nullptr) {
            printf("[ACCEPTANCE][INVARIANT] Running partition must not be nullptr\n");
        }

        if (bootPartition == nullptr) {
            printf("[ACCEPTANCE][INVARIANT] Boot partition must not be nullptr\n");
        }

        if (!partitionsMatch(runningPartition, bootPartition)) {
            printf("[ACCEPTANCE][INVARIANT] Running partition must match boot partition\n");
        }
    }

    if (_context.bootReport == nullptr && _currentState != AcceptanceState::Idle) {
        printf("[ACCEPTANCE][INVARIANT] BootValidationReport must be valid\n");
    }

    // Invariant: Accepted implies ESP-IDF called exactly once
    if (_currentState == AcceptanceState::Accepted && _markValidCallCount != 1) {
        printf("[ACCEPTANCE][INVARIANT] Accepted state requires exactly one mark-valid call, got %u\n", _markValidCallCount);
    }

    // Invariant: Accepted implies rollbackCancelled == true and firmwarePermanent == true
    if (_currentState == AcceptanceState::Accepted) {
        if (!_report.rollbackCancelled) {
            printf("[ACCEPTANCE][INVARIANT] Accepted state requires rollbackCancelled == true\n");
        }
        if (!_report.firmwarePermanent) {
            printf("[ACCEPTANCE][INVARIANT] Accepted state requires firmwarePermanent == true\n");
        }
    }

    if (_context.bootReport != nullptr &&
        _context.bootReport->eligibleForMarkValid &&
        _context.bootReport->state != BootValidation::BootState::ValidationComplete) {
        printf("[ACCEPTANCE][INVARIANT] eligibleForMarkValid requires ValidationComplete\n");
    }

    if (_currentState == AcceptanceState::Accepted &&
        (!_report.rollbackCancelled || !_report.firmwarePermanent)) {
        printf("[ACCEPTANCE][INVARIANT] Accepted requires rollbackCancelled and firmwarePermanent\n");
    }

    if (_currentState == AcceptanceState::Rejected && _markValidCallCount != 0) {
        printf("[ACCEPTANCE][INVARIANT] Rejected requires ESP-IDF API was never called\n");
    }

    if (_currentState == AcceptanceState::Error && !_espApiFailed) {
        printf("[ACCEPTANCE][INVARIANT] Error requires ESP-IDF acceptance failure\n");
    }

    if (_currentState == AcceptanceState::Pending && _markValidCallCount != 0) {
        printf("[ACCEPTANCE][INVARIANT] Pending requires ESP-IDF API was never called\n");
    }

    if (_currentState == AcceptanceState::Accepting &&
        previousState != AcceptanceState::Pending) {
        printf("[ACCEPTANCE][INVARIANT] Accepting requires previous Pending state\n");
    }

    if ((_currentState == AcceptanceState::Accepting ||
         _currentState == AcceptanceState::Accepted ||
         _currentState == AcceptanceState::Error) &&
        (_context.bootReport == nullptr ||
         _context.bootReport->state != BootValidation::BootState::ValidationComplete)) {
        printf("[ACCEPTANCE][INVARIANT] ValidationComplete must precede acceptance\n");
    }

    if ((_currentState == AcceptanceState::Accepting ||
         _currentState == AcceptanceState::Accepted ||
         _currentState == AcceptanceState::Error) &&
        (_context.bootContext == nullptr ||
         _context.bootContext->bootOrigin != BootValidation::BootOrigin::OTA)) {
        printf("[ACCEPTANCE][INVARIANT] BootOrigin must be OTA for acceptance\n");
    }

    if ((_currentState == AcceptanceState::Accepting ||
         _currentState == AcceptanceState::Accepted) &&
        _otaStateImmediate != ESP_OTA_IMG_PENDING_VERIFY) {
        printf("[ACCEPTANCE][INVARIANT] Immediate OTA state must be PendingVerify before mark-valid\n");
    }

    if (_currentState == AcceptanceState::Accepted &&
        _otaStateAfter != ESP_OTA_IMG_VALID) {
        printf("[ACCEPTANCE][INVARIANT] Accepted requires OTA image state Valid after mark-valid\n");
    }
}

void FirmwareAcceptanceManager::logPreconditionFailure(const char* message) const {
    printf("[Acceptance] Preconditions rejected: %s\n",
           message != nullptr ? message : "");
}
#endif

} // namespace Acceptance
