#pragma once

#include <Arduino.h>

#include "config.h"

struct RgbColor {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

enum class LedEffect : uint8_t {
  None = 0,
  Solid,
  Breathing,
  Rainbow,
  Ambient,
};

struct StateSnapshot {
  bool relayStates[Config::RelayCount] = {};
  uint8_t brightness = Config::DefaultBrightness;
  RgbColor color{};
  LedEffect activeEffect = LedEffect::None;
  bool ledsEnabled = false;
};

class StateCache {
 public:
  void begin();

  void applySnapshot(const StateSnapshot& snapshot);
  StateSnapshot snapshot() const;

  void setRelayState(uint8_t relayNumber, bool enabled);
  bool relayState(uint8_t relayNumber) const;

  void setColor(uint8_t r, uint8_t g, uint8_t b);
  RgbColor color() const;

  void setBrightness(uint8_t brightness);
  uint8_t brightness() const;

  void setActiveEffect(LedEffect effect);
  LedEffect activeEffect() const;

  void setLedsEnabled(bool enabled);
  bool ledsEnabled() const;

 private:
  bool relayStates_[Config::RelayCount] = {};
  uint8_t brightness_ = Config::DefaultBrightness;
  RgbColor color_{};
  LedEffect activeEffect_ = LedEffect::None;
  bool ledsEnabled_ = false;
};