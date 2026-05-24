#include "heartbeat.h"

void Heartbeat::begin() {
  lastBeatMs_ = millis();
}

void Heartbeat::update() {
  const uint32_t now = millis();
  if (now - lastBeatMs_ < Config::HeartbeatIntervalMs) {
    return;
  }

  lastBeatMs_ = now;
  // Reserved for future liveness/synchronization messages.
}

