#pragma once

namespace Reboot {

/**
 * @brief Policy hook for deciding whether a pending reboot may execute now.
 */
class IRebootPolicy {
public:
    virtual ~IRebootPolicy() = default;
    virtual bool canReboot() const = 0;
    virtual const char* reason() const = 0;
};

} // namespace Reboot
