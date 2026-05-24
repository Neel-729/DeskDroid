#pragma once

#include <Arduino.h>

class Watchdog {
 public:
  void begin();
  void update();
};

