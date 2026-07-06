#pragma once

#include <Arduino.h>

class IUpdateProvider {
public:
    virtual ~IUpdateProvider() = default;
    
    virtual bool begin() = 0;
    virtual void loop() = 0;
    virtual bool available() const = 0;
    virtual const char* providerName() const = 0;
};