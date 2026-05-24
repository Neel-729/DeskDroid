#include "runtime.h"

#include "config.h"
#include "../utils/logger.h"

Runtime::Runtime(Stream& stream) : stream_(stream) {}

void Runtime::begin() {
  const uint32_t now = millis();
  bootMs_ = now;
  lastHeartbeatMs_ = now;
  state_ = RuntimeState::Booting;
}

void Runtime::update() {
  if (state_ != RuntimeState::Running) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastHeartbeatMs_ >= Config::ConnectionTimeoutMs) {
    transitionTo(RuntimeState::Disconnected);
    Logger::info(stream_, F("[HEARTBEAT]"), F("connection timeout"));
  }
}

void Runtime::markBootReadySent() {
  if (state_ == RuntimeState::Booting) {
    transitionTo(RuntimeState::WaitingForSync);
  }
}

void Runtime::recordHeartbeat() {
  lastHeartbeatMs_ = millis();
  if (state_ == RuntimeState::Disconnected) {
    transitionTo(RuntimeState::WaitingForSync);
  }
}

void Runtime::beginSync() {
  transitionTo(RuntimeState::Syncing);
}

void Runtime::completeSync() {
  lastHeartbeatMs_ = millis();
  transitionTo(RuntimeState::Running);
}

void Runtime::failSync() {
  if (state_ == RuntimeState::Syncing) {
    transitionTo(RuntimeState::WaitingForSync);
  }
}

RuntimeState Runtime::state() const {
  return state_;
}

bool Runtime::isRunning() const {
  return state_ == RuntimeState::Running;
}

bool Runtime::needsSync() const {
  return state_ == RuntimeState::WaitingForSync || state_ == RuntimeState::Disconnected ||
         state_ == RuntimeState::Booting;
}

uint32_t Runtime::uptimeMs() const {
  return millis() - bootMs_;
}

uint32_t Runtime::lastHeartbeatMs() const {
  return lastHeartbeatMs_;
}

void Runtime::transitionTo(RuntimeState state) {
  if (state_ == state) {
    return;
  }

  state_ = state;
  Logger::info(stream_, F("[SYSTEM]"), stateName(state_));
}

const __FlashStringHelper* Runtime::stateName(RuntimeState state) const {
  switch (state) {
    case RuntimeState::Booting:
      return F("BOOTING");
    case RuntimeState::WaitingForSync:
      return F("WAITING_FOR_SYNC");
    case RuntimeState::Syncing:
      return F("SYNCING");
    case RuntimeState::Running:
      return F("RUNNING");
    case RuntimeState::Disconnected:
      return F("DISCONNECTED");
  }

  return F("UNKNOWN");
}
