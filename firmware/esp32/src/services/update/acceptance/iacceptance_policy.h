#pragma once

namespace Acceptance {

class IAcceptancePolicy {
public:
    virtual ~IAcceptancePolicy() = default;
    virtual bool noActiveInstallerSession() const = 0;
    virtual bool noPendingRebootExecution() const = 0;
    virtual const char* reason() const = 0;
};

} // namespace Acceptance
