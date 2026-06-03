#pragma once

#include <Arduino.h>

enum class RuntimeState : uint8_t {
  Booting,
  WaitingForSync,
  Syncing,
  Running,
  Disconnected,
};

class Runtime {
 public:
  explicit Runtime(Stream& stream);

  void begin();
  void update();

  void markBootReadySent();
  void recordHeartbeat();
  void beginSync();
  void completeSync();
  void failSync();
  void recoverFromStall();

  RuntimeState state() const;
  bool isRunning() const;
  bool needsSync() const;
  uint32_t uptimeMs() const;
  uint32_t lastHeartbeatMs() const;
  uint32_t stateChangedAtMs() const;
  uint16_t recoveryCount() const;

 private:
  void transitionTo(RuntimeState state);
  const __FlashStringHelper* stateName(RuntimeState state) const;

  Stream& stream_;
  uint32_t bootMs_ = 0;
  uint32_t lastHeartbeatMs_ = 0;
  uint32_t stateChangedAtMs_ = 0;
  uint16_t recoveryCount_ = 0;
  RuntimeState state_ = RuntimeState::Booting;
};
