#include "timers.h"

IntervalTimer::IntervalTimer(uint32_t intervalMs) : intervalMs_(intervalMs), lastMs_(millis()) {}

void IntervalTimer::setInterval(uint32_t intervalMs) {
  intervalMs_ = intervalMs;
}

void IntervalTimer::reset(uint32_t nowMs) {
  lastMs_ = nowMs;
}

bool IntervalTimer::elapsed(uint32_t nowMs) {
  if (nowMs - lastMs_ < intervalMs_) {
    return false;
  }

  lastMs_ = nowMs;
  return true;
}

