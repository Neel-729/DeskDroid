#include "boot_validation_manager.h"
#include <esp_ota_ops.h>

namespace BootValidation {

BootValidationManager::BootValidationManager()
    : _currentState(BootState::Unknown)
    , _previousState(BootState::Unknown)
    , _policy(nullptr)
    , _initialized(false) {
    clearContext();
    clearHealth();
    initializeReport();
}

bool BootValidationManager::initialize(const IBootValidationPolicy* policy) {
    _policy = policy;
    clearContext();
    clearHealth();
    initializeReport();

    _context.bootTimestampMs = millis();
    _context.runningPartition = esp_ota_get_running_partition();
    _context.bootPartition = esp_ota_get_boot_partition();
    _context.firstBoot = detectFirstBootAfterOTA(
        _context.runningPartition,
        _context.bootPartition,
        &_context.otaPendingValidation);

#ifdef OTA_PLATFORM_VALIDATION
    printf("[BootValidation] Running partition: %s (%p)\n",
           _context.runningPartition != nullptr ? _context.runningPartition->label : "nullptr",
           reinterpret_cast<const void*>(_context.runningPartition));
    printf("[BootValidation] Boot partition: %s (%p)\n",
           _context.bootPartition != nullptr ? _context.bootPartition->label : "nullptr",
           reinterpret_cast<const void*>(_context.bootPartition));
    printf("[BootValidation] Detected boot origin: %s, boot type: %s\n",
           bootOriginToString(_context.bootOrigin),
           _context.firstBoot ? "FirstBootAfterOTA" : "NormalBoot");
    printf("[BootValidation] OTA pending validation: %d timestamp=%llu\n",
           _context.otaPendingValidation,
           static_cast<unsigned long long>(_context.bootTimestampMs));
#endif

    _initialized = true;

    if (_context.firstBoot) {
        transitionTo(BootState::FirstBootAfterOTA);
        _report.result = BootValidationResult::Pending;
        _report.state = _currentState;
        _report.message = "First boot after OTA detected";
    } else {
        transitionTo(BootState::NormalBoot);
        _report.result = BootValidationResult::Passed;
        _report.state = _currentState;
        _report.validationTimestampMs = millis();
        _report.message = "Normal boot detected";
        _report.eligibleForMarkValid = false;
    }

    return _context.runningPartition != nullptr && _context.bootPartition != nullptr;
}

bool BootValidationManager::beginValidation(const IBootValidationPolicy* policy) {
    if (policy != nullptr) {
        _policy = policy;
    }

    if (!_initialized || _currentState != BootState::FirstBootAfterOTA) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[BootValidation] beginValidation rejected state=%s initialized=%d\n",
               bootStateToString(_currentState), _initialized);
#endif
        return false;
    }

    const bool policyAllowsValidation = _policy == nullptr || _policy->shouldValidate();
    const char* policyReason = _policy == nullptr ? "No policy" : _policy->reason();

#ifdef OTA_PLATFORM_VALIDATION
    printf("[BootValidation] Policy shouldValidate=%d reason=%s\n",
           policyAllowsValidation, policyReason != nullptr ? policyReason : "");
#endif

    if (!policyAllowsValidation) {
        _report.result = BootValidationResult::Pending;
        _report.state = _currentState;
        _report.validationTimestampMs = millis();
        _report.message = policyReason != nullptr ? policyReason : "Boot validation deferred by policy";
        _report.eligibleForMarkValid = false;
        return false;
    }

    transitionTo(BootState::Validating);
    _report.result = BootValidationResult::Pending;
    _report.state = _currentState;
    _report.validationTimestampMs = millis();
    _report.message = "Boot validation in progress";
    _report.eligibleForMarkValid = false;
    return true;
}

void BootValidationManager::update(const BootHealth& health) {
    _health = health;

    // Calculate health mask and counters
    uint8_t mask = 0;
    uint8_t passed = 0;
    uint8_t failed = 0;
    
    // Bit positions match the order of health checks in allRequiredHealthChecksPassed
    if (health.schedulerRunning) { mask |= (1 << 0); passed++; } else { failed++; }
    if (health.servicesInitialized) { mask |= (1 << 1); passed++; } else { failed++; }
    if (health.filesystemMounted) { mask |= (1 << 2); passed++; } else { failed++; }
    if (health.preferencesAvailable) { mask |= (1 << 3); passed++; } else { failed++; }
    if (health.rtcInitialized) { mask |= (1 << 4); passed++; } else { failed++; }
    if (health.displayInitialized) { mask |= (1 << 5); passed++; } else { failed++; }
    if (health.communicationReady) { mask |= (1 << 6); passed++; } else { failed++; }
    
    _report.healthMask = mask;
    _report.passedChecks = passed;
    _report.failedChecks = failed;

#ifdef OTA_PLATFORM_VALIDATION
    printf("[BootValidation] Current BootState=%s\n", bootStateToString(_currentState));
    logHealthChecks(_health);
    printf("[BootValidation] Health mask=0x%02X passed=%d failed=%d\n", mask, passed, failed);
#endif

    if (_currentState != BootState::Validating) {
        return;
    }

    const bool healthy = allRequiredHealthChecksPassed(_health);

    if (healthy) {
        transitionTo(BootState::Healthy);
        _report.result = BootValidationResult::Passed;
        _report.message = "Boot health validation passed";
    } else {
        transitionTo(BootState::Unhealthy);
        _report.result = BootValidationResult::Failed;
        _report.message = "Boot health validation failed";
    }

    _report.state = _currentState;
    _report.validationTimestampMs = millis();
    _report.eligibleForMarkValid =
        healthy && _policy != nullptr && _policy->allowMarkValid();

#ifdef OTA_PLATFORM_VALIDATION
    printf("[BootValidation] Validation result=%s timestamp=%llu eligibleForMarkValid=%d\n",
           bootValidationResultToString(_report.result),
           static_cast<unsigned long long>(_report.validationTimestampMs),
           _report.eligibleForMarkValid);
#endif

    transitionTo(BootState::ValidationComplete);
    _report.state = _currentState;
}

const BootValidationReport& BootValidationManager::report() const {
    return _report;
}

BootState BootValidationManager::state() const {
    return _currentState;
}

bool BootValidationManager::isHealthy() const {
    return _report.result == BootValidationResult::Passed &&
           allRequiredHealthChecksPassed(_health);
}

void BootValidationManager::reset() {
#ifdef OTA_PLATFORM_VALIDATION
    printf("[BootValidation] Reset requested from state=%s\n", bootStateToString(_currentState));
#endif

    clearContext();
    clearHealth();
    initializeReport();
    _policy = nullptr;
    _initialized = false;
    transitionTo(BootState::Unknown);
}

void BootValidationManager::transitionTo(BootState newState) {
    if (_currentState == newState) {
#ifdef OTA_PLATFORM_VALIDATION
        validateInvariants(_previousState);
#endif
        return;
    }

    const BootState previous = _currentState;

#ifdef OTA_PLATFORM_VALIDATION
    printf("[BootValidation] State transition: %s -> %s\n",
           bootStateToString(previous), bootStateToString(newState));
#endif

    _previousState = previous;
    _currentState = newState;

#ifdef OTA_PLATFORM_VALIDATION
    validateInvariants(previous);
#endif
}

void BootValidationManager::clearContext() {
    _context.firstBoot = false;
    _context.otaPendingValidation = false;
    _context.bootTimestampMs = 0;
    _context.runningPartition = nullptr;
    _context.bootPartition = nullptr;
    _context.bootOrigin = BootOrigin::Unknown;
}

void BootValidationManager::clearHealth() {
    _health.schedulerRunning = false;
    _health.servicesInitialized = false;
    _health.filesystemMounted = false;
    _health.preferencesAvailable = false;
    _health.rtcInitialized = false;
    _health.displayInitialized = false;
    _health.communicationReady = false;
}

void BootValidationManager::initializeReport() {
    _report.result = BootValidationResult::Unknown;
    _report.state = BootState::Unknown;
    _report.validationTimestampMs = 0;
    _report.message = "";
    _report.eligibleForMarkValid = false;
    _report.failedChecks = 0;
    _report.passedChecks = 0;
    _report.healthMask = 0;
}

bool BootValidationManager::allRequiredHealthChecksPassed(const BootHealth& health) const {
    return health.schedulerRunning &&
           health.servicesInitialized &&
           health.filesystemMounted &&
           health.preferencesAvailable &&
           health.rtcInitialized &&
           health.displayInitialized &&
           health.communicationReady;
}

bool BootValidationManager::anyRequiredHealthCheckFailed(const BootHealth& health) const {
    return !allRequiredHealthChecksPassed(health);
}

bool BootValidationManager::detectFirstBootAfterOTA(
    const esp_partition_t* runningPartition,
    const esp_partition_t* bootPartition,
    bool* otaPendingValidation) {
    if (otaPendingValidation != nullptr) {
        *otaPendingValidation = false;
    }

    if (runningPartition == nullptr || bootPartition == nullptr) {
        _context.bootOrigin = BootOrigin::Unknown;
        return false;
    }

    const bool runningIsBoot =
        runningPartition == bootPartition ||
        runningPartition->address == bootPartition->address;
    const bool runningIsFactory =
        runningPartition->type == ESP_PARTITION_TYPE_APP &&
        runningPartition->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY;
    const bool runningIsOta =
        runningPartition->type == ESP_PARTITION_TYPE_APP &&
        runningPartition->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
        runningPartition->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_15;

    esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
    const esp_err_t stateResult = esp_ota_get_state_partition(runningPartition, &otaState);
    const bool pendingValidation =
        stateResult == ESP_OK && otaState == ESP_OTA_IMG_PENDING_VERIFY;

    // Set boot origin based on partition type and state
    if (runningIsFactory) {
        _context.bootOrigin = BootOrigin::Factory;
    } else if (runningIsOta && pendingValidation) {
        _context.bootOrigin = BootOrigin::OTA;
    } else if (runningIsOta && otaState == ESP_OTA_IMG_VALID) {
        // Could be rollback if we previously rolled back - for now mark as OTA
        _context.bootOrigin = BootOrigin::OTA;
    } else {
        _context.bootOrigin = BootOrigin::Unknown;
    }

    if (otaPendingValidation != nullptr) {
        *otaPendingValidation = pendingValidation;
    }

#ifdef OTA_PLATFORM_VALIDATION
    printf("[BootValidation] Boot origin=%s runningIsBoot=%d runningIsFactory=%d runningIsOta=%d otaStateResult=0x%x otaState=%d\n",
           bootOriginToString(_context.bootOrigin), runningIsBoot, runningIsFactory, runningIsOta, stateResult, static_cast<int>(otaState));
#endif

    return runningIsBoot && runningIsOta && pendingValidation;
}

#ifdef OTA_PLATFORM_VALIDATION
void BootValidationManager::logHealthChecks(const BootHealth& health) const {
    printf("[BootValidation] Health schedulerRunning=%d\n", health.schedulerRunning);
    printf("[BootValidation] Health servicesInitialized=%d\n", health.servicesInitialized);
    printf("[BootValidation] Health filesystemMounted=%d\n", health.filesystemMounted);
    printf("[BootValidation] Health preferencesAvailable=%d\n", health.preferencesAvailable);
    printf("[BootValidation] Health rtcInitialized=%d\n", health.rtcInitialized);
    printf("[BootValidation] Health displayInitialized=%d\n", health.displayInitialized);
    printf("[BootValidation] Health communicationReady=%d\n", health.communicationReady);
}

void BootValidationManager::validateInvariants(BootState previousState) const {
    if (_initialized && _context.runningPartition == nullptr) {
        printf("[BOOT][INVARIANT] Running partition must not be nullptr\n");
    }

    if (_initialized && _context.bootPartition == nullptr) {
        printf("[BOOT][INVARIANT] Boot partition must not be nullptr\n");
    }

    if (_report.message == nullptr) {
        printf("[BOOT][INVARIANT] Validation report must always be initialized\n");
    }

    if (_currentState == BootState::Healthy && !allRequiredHealthChecksPassed(_health)) {
        printf("[BOOT][INVARIANT] Healthy requires all required services ready\n");
    }

    if (_currentState == BootState::Unhealthy && !anyRequiredHealthCheckFailed(_health)) {
        printf("[BOOT][INVARIANT] Unhealthy requires at least one failed check\n");
    }

    if (_currentState == BootState::ValidationComplete &&
        previousState != BootState::Healthy &&
        previousState != BootState::Unhealthy) {
        printf("[BOOT][INVARIANT] ValidationComplete requires previous Healthy or Unhealthy state\n");
    }

    // Verify BootOrigin is consistent with OTA image state
    if (_context.otaPendingValidation && _context.bootOrigin != BootOrigin::OTA) {
        printf("[BOOT][INVARIANT] OTA pending validation requires BootOrigin::OTA, got %s\n", 
               bootOriginToString(_context.bootOrigin));
    }

    // Verify eligibleForMarkValid is only true after Healthy
    if (_report.eligibleForMarkValid && _currentState != BootState::ValidationComplete) {
        printf("[BOOT][INVARIANT] eligibleForMarkValid may only be true after ValidationComplete\n");
    }

    if (_report.eligibleForMarkValid && _report.result != BootValidationResult::Passed) {
        printf("[BOOT][INVARIANT] eligibleForMarkValid requires validation result Passed\n");
    }
}
#endif

} // namespace BootValidation