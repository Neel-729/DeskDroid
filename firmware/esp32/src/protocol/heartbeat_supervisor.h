#pragma once

#include <Arduino.h>

class HeartbeatSupervisor {
 public:
  void begin(unsigned long now);

  bool shouldSendPing(unsigned long now) const;
  void markPingSent(unsigned long now);
  void recordPong(unsigned long now);
  bool timedOut(unsigned long now) const;

  unsigned long lastPongMs() const;

 private:
  unsigned long lastPingMs_ = 0;
  unsigned long lastPongMs_ = 0;
};

