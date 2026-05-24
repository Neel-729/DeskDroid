#pragma once

#include <Arduino.h>

class Runtime {
 public:
  void begin();
  void update();

  uint32_t uptimeMs() const;

 private:
  uint32_t bootMs_ = 0;
};

