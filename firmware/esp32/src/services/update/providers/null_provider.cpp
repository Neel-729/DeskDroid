#include "null_provider.h"

bool NullUpdateProvider::begin() {
    return true;
}

void NullUpdateProvider::loop() {
    // No-op - no work to perform
}

bool NullUpdateProvider::available() const {
    return false;
}

const char* NullUpdateProvider::providerName() const {
    return "None";
}