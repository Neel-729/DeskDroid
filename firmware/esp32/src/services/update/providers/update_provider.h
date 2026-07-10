#pragma once

#include <Arduino.h>
#include "../models/update_decision.h"
#include "../models/update_error.h"
#include "../models/update_info.h"

class IUpdateProvider {
public:
    virtual ~IUpdateProvider() = default;
    
    virtual bool begin() = 0;
    virtual void loop() = 0;
    virtual bool available() const = 0;
    virtual const char* providerName() const = 0;
    virtual UpdateDecisionContext checkForUpdate() = 0;
    virtual const UpdateInfo& latestUpdateInfo() const = 0;
    virtual const UpdateError& lastError() const = 0;
    virtual void reset() = 0;
};
