#include "runtime.h"

void Runtime::begin() {
  bootMs_ = millis();
}

void Runtime::update() {}

uint32_t Runtime::uptimeMs() const {
  return millis() - bootMs_;
}

