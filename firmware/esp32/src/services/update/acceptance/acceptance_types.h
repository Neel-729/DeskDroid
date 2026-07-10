#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "../boot/boot_validation_types.h"
#include "../boot/iboot_validation_policy.h"

namespace Acceptance {

enum class AcceptanceState : uint8_t {
    Idle,
    Pending,
    Accepting,
    Accepted,
    Rejected,
    Error
};

enum class AcceptanceResult : uint8_t {
    Unknown,
    Success,
    Failure,
    Skipped
};

struct AcceptanceReport {
    AcceptanceResult result;
    AcceptanceState state;
    uint64_t timestampMs;
    const char* message;
    bool rollbackCancelled;
    bool firmwarePermanent;
};

struct AcceptanceContext {
    const BootValidation::BootValidationReport* bootReport;
    const BootValidation::BootContext* bootContext;
    const BootValidation::IBootValidationPolicy* bootPolicy;
};

inline const char* acceptanceStateToString(AcceptanceState state) {
    switch (state) {
        case AcceptanceState::Idle: return "Idle";
        case AcceptanceState::Pending: return "Pending";
        case AcceptanceState::Accepting: return "Accepting";
        case AcceptanceState::Accepted: return "Accepted";
        case AcceptanceState::Rejected: return "Rejected";
        case AcceptanceState::Error: return "Error";
        default: return "Unknown";
    }
}

inline const char* acceptanceResultToString(AcceptanceResult result) {
    switch (result) {
        case AcceptanceResult::Unknown: return "Unknown";
        case AcceptanceResult::Success: return "Success";
        case AcceptanceResult::Failure: return "Failure";
        case AcceptanceResult::Skipped: return "Skipped";
        default: return "Unknown";
    }
}

} // namespace Acceptance
