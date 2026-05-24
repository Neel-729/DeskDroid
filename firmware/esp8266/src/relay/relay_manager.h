#pragma once

#include <Arduino.h>

#include "config.h"
#include "../state/state_cache.h"

class RelayManager {
 public:
  explicit RelayManager(StateCache& state);

  void begin();
  void update();

  bool setRelay(uint8_t relayNumber, bool enabled);
  void applyStateCache();
  void allOff();

 private:
  void applyRelay(uint8_t index, bool enabled);

  StateCache& state_;
  bool desiredStates_[Config::RelayCount] = {};
  bool appliedStates_[Config::RelayCount] = {};
  bool dirty_[Config::RelayCount] = {};
};
