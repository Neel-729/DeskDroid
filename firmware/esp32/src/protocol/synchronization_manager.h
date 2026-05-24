#pragma once

#include <Arduino.h>

class SynchronizationManager {
 public:
  void begin();
  void requestSync();
  bool pending() const;
  bool inFlight() const;
  bool shouldSend(unsigned long now) const;
  void markSent(unsigned long now);
  void complete();
  void reset();
  bool retryLimitReached() const;

 private:
  bool pending_ = false;
  bool inFlight_ = false;
  uint8_t retries_ = 0;
  unsigned long lastAttemptMs_ = 0;
};

