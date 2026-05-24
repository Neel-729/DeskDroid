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
  void allOff();

 private:
  StateCache& state_;
};

