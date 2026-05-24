#pragma once

#include <Arduino.h>

class AnimationRuntime {
 public:
  void begin();
  void update();

  uint32_t frameCount() const;

 private:
  uint32_t frameCount_ = 0;
};

