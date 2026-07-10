#pragma once

#include <Arduino.h>
#include "ireboot_policy.h"
#include "reboot_types.h"

namespace Reboot {

/**
 * @brief Standalone reboot request state machine.
 *
 * Owns only reboot scheduling and explicit reboot execution. It has no
 * dependency on OTA provider, transport, verification, or installer internals.
 */
class RebootController {
public:
    RebootController();

    bool requestReboot(RebootReason reason, bool immediate);
    bool cancelPendingReboot();
    bool hasPendingReboot() const;
    RebootState state() const;
    const RebootRequest& pendingRequest() const;
    RebootResult executePendingReboot(const IRebootPolicy* policy = nullptr);
    void reset();

private:
    void transitionTo(RebootState newState);
    void clearPendingRequest();
    void clearLastResult();

#ifdef OTA_PLATFORM_VALIDATION
    void validateInvariants(RebootState previousState) const;
#endif

    RebootState _currentState;
    RebootState _previousState;
    RebootRequest _pendingRequest;
    RebootResult _lastResult;
};

} // namespace Reboot
