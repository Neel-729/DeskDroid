#pragma once

#include <Arduino.h>

#include "config.h"

class Heartbeat {
 public:
  void begin();
  void update();

 private:
  uint32_t lastBeatMs_ = 0;
};

