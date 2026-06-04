#include "state_cache.h"

void StateCache::begin() {
  for (uint8_t i = 0; i < Config::RelayCount; ++i) {
    relayStates_[i] = false;
  }
  brightness_ = Config::DefaultBrightness;
  color_ = {};
  activeEffect_ = LedEffect::None;
  ledsEnabled_ = false;
}

void StateCache::applySnapshot(const StateSnapshot& snapshot) {
  for (uint8_t i = 0; i < Config::RelayCount; ++i) {
    relayStates_[i] = snapshot.relayStates[i];
  }
}

StateSnapshot StateCache::snapshot() const {
  StateSnapshot result;
  for (uint8_t i = 0; i < Config::RelayCount; ++i) {
    result.relayStates[i] = relayStates_[i];
  }
  result.brightness = brightness_;
  result.color = color_;
  result.activeEffect = activeEffect_;
  result.ledsEnabled = ledsEnabled_;
  return result;
}

void StateCache::setRelayState(uint8_t relayNumber, bool enabled) {
  if (relayNumber == 0 || relayNumber > Config::RelayCount) {
    return;
  }
  relayStates_[relayNumber - 1] = enabled;
}

bool StateCache::relayState(uint8_t relayNumber) const {
  if (relayNumber == 0 || relayNumber > Config::RelayCount) {
    return false;
  }
  return relayStates_[relayNumber - 1];
}

void StateCache::setColor(uint8_t r, uint8_t g, uint8_t b) {
  color_ = {r, g, b};
}

RgbColor StateCache::color() const {
  return color_;
}

void StateCache::setBrightness(uint8_t brightness) {
  brightness_ = brightness;
}

uint8_t StateCache::brightness() const {
  return brightness_;
}

void StateCache::setActiveEffect(LedEffect effect) {
  activeEffect_ = effect;
}

LedEffect StateCache::activeEffect() const {
  return activeEffect_;
}

void StateCache::setLedsEnabled(bool enabled) {
  ledsEnabled_ = enabled;
}

bool StateCache::ledsEnabled() const {
  return ledsEnabled_;
}
