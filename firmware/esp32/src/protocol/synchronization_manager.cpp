#include "synchronization_manager.h"

#include "config.h"

void SynchronizationManager::begin(){
  pending_ = true;
  inFlight_ = false;
  retries_ = 0;
  lastAttemptMs_ = 0;
}

void SynchronizationManager::requestSync(){
  pending_ = true;
}

bool SynchronizationManager::pending() const {
  return pending_;
}

bool SynchronizationManager::inFlight() const {
  return inFlight_;
}

bool SynchronizationManager::shouldSend(unsigned long now) const {
  if(!pending_ && !inFlight_) return false;
  if(!inFlight_) return true;
  return now - lastAttemptMs_ >= Config::Esp8266SyncRetryIntervalMs;
}

void SynchronizationManager::markSent(unsigned long now){
  inFlight_ = true;
  pending_ = false;
  lastAttemptMs_ = now;
  if(retries_ < 255) retries_++;
}

void SynchronizationManager::complete(){
  inFlight_ = false;
  retries_ = 0;
}

void SynchronizationManager::reset(){
  pending_ = true;
  inFlight_ = false;
  retries_ = 0;
  lastAttemptMs_ = 0;
}

bool SynchronizationManager::retryLimitReached() const {
  return retries_ >= Config::Esp8266MaxSyncRetries;
}

uint8_t SynchronizationManager::retries() const {
  return retries_;
}
