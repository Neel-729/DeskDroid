#include "watchdog.h"

#include "config.h"
#include "runtime.h"
#include "../protocol/command_queue.h"
#include "../protocol/protocol.h"
#include "../protocol/protocol_responses.h"

namespace {
constexpr uint32_t WatchdogCheckIntervalMs = 500;
constexpr uint32_t SyncStallTimeoutMs = Config::ConnectionTimeoutMs;
constexpr uint32_t RuntimeStallTimeoutMs = Config::ConnectionTimeoutMs * 2UL;
}

void Watchdog::begin() {
  lastCheckMs_ = millis();
}

void Watchdog::begin(Runtime& runtime, Protocol& protocol, CommandQueue& commandQueue, Stream& stream) {
  runtime_ = &runtime;
  protocol_ = &protocol;
  commandQueue_ = &commandQueue;
  stream_ = &stream;
  begin();
}

void Watchdog::update() {
  ESP.wdtFeed();

  if(runtime_ == nullptr || protocol_ == nullptr || commandQueue_ == nullptr || stream_ == nullptr){
    return;
  }

  const uint32_t now = millis();
  if(now - lastCheckMs_ < WatchdogCheckIntervalMs){
    return;
  }
  lastCheckMs_ = now;

  if(runtime_->state() == RuntimeState::Syncing &&
     now - runtime_->stateChangedAtMs() >= SyncStallTimeoutMs){
    recover(ProtocolError::SyncTimeout);
    return;
  }

  if(runtime_->state() == RuntimeState::Running &&
     now - runtime_->lastHeartbeatMs() >= RuntimeStallTimeoutMs){
    recover(ProtocolError::RuntimeStalled);
    return;
  }

  if(runtime_->state() == RuntimeState::Disconnected &&
     now - runtime_->stateChangedAtMs() >= Config::ConnectionTimeoutMs){
    recover(ProtocolError::RuntimeStalled);
    return;
  }
}

void Watchdog::recover(ProtocolError error) {
  ProtocolResponse::sendError(*stream_, error);
  protocol_->reset();
  commandQueue_->clear();
  runtime_->recoverFromStall();
}
