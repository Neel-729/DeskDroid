#pragma once

#include "update_provider.h"

class NullUpdateProvider : public IUpdateProvider {
public:
    bool begin() override;
    void loop() override;
    bool available() const override;
    const char* providerName() const override;
};