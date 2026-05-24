#include "heartbeat_supervisor.h"

#include "config.h"

void HeartbeatSupervisor::begin(unsigned long now){
  lastPingMs_ = 0;
  lastPongMs_ = now;
}

bool HeartbeatSupervisor::shouldSendPing(unsigned long now) const {
  return lastPingMs_ == 0 || now - lastPingMs_ >= Config::Esp8266HeartbeatIntervalMs;
}

void HeartbeatSupervisor::markPingSent(unsigned long now){
  lastPingMs_ = now;
}

void HeartbeatSupervisor::recordPong(unsigned long now){
  lastPongMs_ = now;
}

bool HeartbeatSupervisor::timedOut(unsigned long now) const {
  return lastPongMs_ != 0 && now - lastPongMs_ >= Config::Esp8266HeartbeatTimeoutMs;
}

unsigned long HeartbeatSupervisor::lastPongMs() const {
  return lastPongMs_;
}

