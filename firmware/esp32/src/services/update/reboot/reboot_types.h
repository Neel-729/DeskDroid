#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace Reboot {

/**
 * @brief Reboot controller state machine states.
 */
enum class RebootState : uint8_t {
    Idle,
    Pending,
    Executing,
    Completed,
    Cancelled
};

/**
 * @brief Reason a reboot was requested.
 */
enum class RebootReason : uint8_t {
    None,
    OTAActivation,
    Manual,
    FutureReserved
};

/**
 * @brief Lightweight reboot request descriptor.
 */
struct RebootRequest {
    bool valid;
    RebootReason reason;
    uint64_t requestTimestampMs;
    bool immediate;
};

/**
 * @brief Result returned by explicit reboot execution.
 */
struct RebootResult {
    bool success;
    bool rebootExecuted;
    const char* message;
    uint64_t executionTimestampMs;
};

inline const char* rebootStateToString(RebootState state) {
    switch (state) {
        case RebootState::Idle: return "Idle";
        case RebootState::Pending: return "Pending";
        case RebootState::Executing: return "Executing";
        case RebootState::Completed: return "Completed";
        case RebootState::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

inline const char* rebootReasonToString(RebootReason reason) {
    switch (reason) {
        case RebootReason::None: return "None";
        case RebootReason::OTAActivation: return "OTAActivation";
        case RebootReason::Manual: return "Manual";
        case RebootReason::FutureReserved: return "FutureReserved";
        default: return "Unknown";
    }
}

} // namespace Reboot
