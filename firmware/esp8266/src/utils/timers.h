#pragma once

#include <Arduino.h>

class IntervalTimer {
 public:
  explicit IntervalTimer(uint32_t intervalMs = 0);

  void setInterval(uint32_t intervalMs);
  void reset(uint32_t nowMs = millis());
  bool elapsed(uint32_t nowMs = millis());

 private:
  uint32_t intervalMs_ = 0;
  uint32_t lastMs_ = 0;
};

