#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <esp_partition.h>

namespace BootValidation {

enum class BootOrigin : uint8_t {
    Unknown,
    Factory,
    OTA,
    Rollback
};

enum class BootState : uint8_t {
    Unknown,
    NormalBoot,
    FirstBootAfterOTA,
    Validating,
    Healthy,
    Unhealthy,
    ValidationComplete
};

enum class BootValidationResult : uint8_t {
    Unknown,
    Pending,
    Passed,
    Failed
};

struct BootContext {
    bool firstBoot;
    bool otaPendingValidation;
    uint64_t bootTimestampMs;
    const esp_partition_t* runningPartition;
    const esp_partition_t* bootPartition;
    BootOrigin bootOrigin;
};

struct BootHealth {
    bool schedulerRunning;
    bool servicesInitialized;
    bool filesystemMounted;
    bool preferencesAvailable;
    bool rtcInitialized;
    bool displayInitialized;
    bool communicationReady;
};

struct BootValidationReport {
    BootValidationResult result;
    BootState state;
    uint64_t validationTimestampMs;
    const char* message;
    bool eligibleForMarkValid;
    uint8_t failedChecks;
    uint8_t passedChecks;
    uint8_t healthMask;
};

inline const char* bootOriginToString(BootOrigin origin) {
    switch (origin) {
        case BootOrigin::Unknown: return "Unknown";
        case BootOrigin::Factory: return "Factory";
        case BootOrigin::OTA: return "OTA";
        case BootOrigin::Rollback: return "Rollback";
        default: return "Unknown";
    }
}

inline const char* bootStateToString(BootState state) {
    switch (state) {
        case BootState::Unknown: return "Unknown";
        case BootState::NormalBoot: return "NormalBoot";
        case BootState::FirstBootAfterOTA: return "FirstBootAfterOTA";
        case BootState::Validating: return "Validating";
        case BootState::Healthy: return "Healthy";
        case BootState::Unhealthy: return "Unhealthy";
        case BootState::ValidationComplete: return "ValidationComplete";
        default: return "Unknown";
    }
}

inline const char* bootValidationResultToString(BootValidationResult result) {
    switch (result) {
        case BootValidationResult::Unknown: return "Unknown";
        case BootValidationResult::Pending: return "Pending";
        case BootValidationResult::Passed: return "Passed";
        case BootValidationResult::Failed: return "Failed";
        default: return "Unknown";
    }
}

} // namespace BootValidation