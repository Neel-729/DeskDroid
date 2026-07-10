#include "reboot_controller.h"
#include <esp_system.h>

namespace Reboot {

RebootController::RebootController()
    : _currentState(RebootState::Idle)
    , _previousState(RebootState::Idle) {
    clearPendingRequest();
    clearLastResult();
}

bool RebootController::requestReboot(RebootReason reason, bool immediate) {
#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] Request received reason=%s immediate=%d timestamp=%llu\n",
           rebootReasonToString(reason), immediate,
           static_cast<unsigned long long>(millis()));
#endif

    if (_currentState == RebootState::Pending || _currentState == RebootState::Executing) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[RebootController] Reboot request rejected: state=%s\n",
               rebootStateToString(_currentState));
#endif
        return false;
    }

    if (_currentState != RebootState::Idle || reason == RebootReason::None) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[RebootController] Reboot request rejected: state=%s reason=%s\n",
               rebootStateToString(_currentState), rebootReasonToString(reason));
#endif
        return false;
    }

    clearLastResult();
    _pendingRequest.valid = true;
    _pendingRequest.reason = reason;
    _pendingRequest.requestTimestampMs = millis();
    _pendingRequest.immediate = immediate;

    transitionTo(RebootState::Pending);
    return true;
}

bool RebootController::cancelPendingReboot() {
#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] Cancellation requested state=%s\n",
           rebootStateToString(_currentState));
#endif

    if (_currentState != RebootState::Pending) {
        return false;
    }

    clearPendingRequest();
    clearLastResult();
    transitionTo(RebootState::Cancelled);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] Pending reboot cancelled\n");
#endif
    return true;
}

bool RebootController::hasPendingReboot() const {
    return _currentState == RebootState::Pending && _pendingRequest.valid;
}

RebootState RebootController::state() const {
    return _currentState;
}

const RebootRequest& RebootController::pendingRequest() const {
    return _pendingRequest;
}

RebootResult RebootController::executePendingReboot(const IRebootPolicy* policy) {
    RebootResult result = {};
    result.success = false;
    result.rebootExecuted = false;
    result.message = "";
    result.executionTimestampMs = 0;

#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] executePendingReboot() state=%s request.valid=%d\n",
           rebootStateToString(_currentState), _pendingRequest.valid);
#endif

    if (_currentState != RebootState::Pending) {
        result.message = "No pending reboot request";
        _lastResult = result;
        return result;
    }

    if (!_pendingRequest.valid) {
        result.message = "Pending reboot request is invalid";
        _lastResult = result;
        return result;
    }

    const bool policyAllowed = policy == nullptr || policy->canReboot();
    const char* policyReason = policy == nullptr ? "No policy" : policy->reason();

#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] Policy result=%d reason=%s\n",
           policyAllowed, policyReason != nullptr ? policyReason : "");
#endif

    if (!policyAllowed) {
        result.message = policyReason != nullptr ? policyReason : "Reboot denied by policy";
        _lastResult = result;
        return result;
    }

    transitionTo(RebootState::Executing);

    result.success = true;
    result.rebootExecuted = true;
    result.message = "Executing requested reboot";
    result.executionTimestampMs = millis();
    _lastResult = result;

#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] Reboot execution reason=%s timestamp=%llu\n",
           rebootReasonToString(_pendingRequest.reason),
           static_cast<unsigned long long>(result.executionTimestampMs));
#endif

    clearPendingRequest();
    transitionTo(RebootState::Completed);

    esp_restart();
    return result;
}

void RebootController::reset() {
#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] Reset requested from state=%s\n",
           rebootStateToString(_currentState));
#endif

    if (_currentState == RebootState::Pending || _currentState == RebootState::Executing) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[RebootController] Reset rejected while state=%s\n",
               rebootStateToString(_currentState));
#endif
        return;
    }

    clearPendingRequest();
    clearLastResult();
    transitionTo(RebootState::Idle);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] Reset complete\n");
#endif
}

void RebootController::transitionTo(RebootState newState) {
    if (_currentState == newState) {
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return;
    }

    const RebootState previous = _currentState;

#ifdef OTA_PLATFORM_VALIDATION
    printf("[RebootController] State transition: %s -> %s\n",
           rebootStateToString(previous), rebootStateToString(newState));
#endif

    _previousState = previous;
    _currentState = newState;

#ifdef OTA_PLATFORM_VALIDATION
    validateInvariants(previous);
#endif
}

void RebootController::clearPendingRequest() {
    _pendingRequest.valid = false;
    _pendingRequest.reason = RebootReason::None;
    _pendingRequest.requestTimestampMs = 0;
    _pendingRequest.immediate = false;
}

void RebootController::clearLastResult() {
    _lastResult.success = false;
    _lastResult.rebootExecuted = false;
    _lastResult.message = "";
    _lastResult.executionTimestampMs = 0;
}

#ifdef OTA_PLATFORM_VALIDATION
void RebootController::validateInvariants(RebootState previousState) const {
    if (_currentState == RebootState::Pending && !_pendingRequest.valid) {
        printf("[REBOOT][INVARIANT] Pending requires request.valid == true\n");
    }

    if (_currentState == RebootState::Executing && previousState != RebootState::Pending) {
        printf("[REBOOT][INVARIANT] Executing requires previous state Pending\n");
    }

    if (_currentState == RebootState::Completed && !_lastResult.rebootExecuted) {
        printf("[REBOOT][INVARIANT] Completed requires rebootExecuted == true\n");
    }

    if (_currentState == RebootState::Idle && _pendingRequest.valid) {
        printf("[REBOOT][INVARIANT] Idle requires request.valid == false\n");
    }

    if (_currentState == RebootState::Cancelled && _lastResult.rebootExecuted) {
        printf("[REBOOT][INVARIANT] Cancelled requires reboot not executed\n");
    }
}
#endif

} // namespace Reboot
