#pragma once

namespace BootValidation {

class IBootValidationPolicy {
public:
    virtual ~IBootValidationPolicy() = default;
    virtual bool shouldValidate() const = 0;
    virtual bool allowMarkValid() const = 0;
    virtual const char* reason() const = 0;
};

} // namespace BootValidation
