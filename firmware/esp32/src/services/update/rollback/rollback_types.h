#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "../boot/boot_validation_types.h"
#include "../acceptance/acceptance_types.h"

namespace Rollback {

enum class RollbackState : uint8_t {
    Idle,
    Evaluating,
    ReadyForRollback,
    Executing,
    Completed,
    Rejected,
    Error
};

enum class RollbackResult : uint8_t {
    Unknown,
    Ready,
    Rejected,
    Success,
    Failure
};

enum class RollbackReason : uint8_t {
    None,
    BootValidationFailed,
    AcceptanceFailed,
    PendingVerifyExpired,
    ImageInvalid,
    RollbackUnavailable,
    PolicyDenied,
    ExecutionFailed,
    VerificationFailed,
    Unknown
};

class IRollbackPolicy {
public:
    virtual ~IRollbackPolicy() = default;
    virtual bool permitRollback(RollbackReason reason) const = 0;
    virtual bool noActiveInstallerSession() const = 0;
    virtual bool noActiveOtaWriteSession() const = 0;
    virtual bool noPendingFirmwareAcceptance() const = 0;
    virtual const char* reason() const = 0;
};

struct RollbackContext {
    const BootValidation::BootContext* bootContext;
    const BootValidation::BootValidationReport* bootReport;
    const Acceptance::AcceptanceReport* acceptanceReport;
    const IRollbackPolicy* policy;
};

struct RollbackReport {
    RollbackResult result;
    RollbackState state;
    RollbackReason reason;
    bool rollbackPossible;
    bool rollbackRequired;
    bool rollbackExecuted;
    bool rebootTriggered;
    bool rollbackSuccessful;
    uint32_t timestamp;
    const char* message;
};

inline const char* rollbackStateToString(RollbackState state) {
    switch (state) {
        case RollbackState::Idle: return "Idle";
        case RollbackState::Evaluating: return "Evaluating";
        case RollbackState::ReadyForRollback: return "ReadyForRollback";
        case RollbackState::Executing: return "Executing";
        case RollbackState::Completed: return "Completed";
        case RollbackState::Rejected: return "Rejected";
        case RollbackState::Error: return "Error";
        default: return "Unknown";
    }
}

inline const char* rollbackResultToString(RollbackResult result) {
    switch (result) {
        case RollbackResult::Unknown: return "Unknown";
        case RollbackResult::Ready: return "Ready";
        case RollbackResult::Rejected: return "Rejected";
        case RollbackResult::Success: return "Success";
        case RollbackResult::Failure: return "Failure";
        default: return "Unknown";
    }
}

inline const char* rollbackReasonToString(RollbackReason reason) {
    switch (reason) {
        case RollbackReason::None: return "None";
        case RollbackReason::BootValidationFailed: return "BootValidationFailed";
        case RollbackReason::AcceptanceFailed: return "AcceptanceFailed";
        case RollbackReason::PendingVerifyExpired: return "PendingVerifyExpired";
        case RollbackReason::ImageInvalid: return "ImageInvalid";
        case RollbackReason::RollbackUnavailable: return "RollbackUnavailable";
        case RollbackReason::PolicyDenied: return "PolicyDenied";
        case RollbackReason::ExecutionFailed: return "ExecutionFailed";
        case RollbackReason::VerificationFailed: return "VerificationFailed";
        case RollbackReason::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

} // namespace Rollback
