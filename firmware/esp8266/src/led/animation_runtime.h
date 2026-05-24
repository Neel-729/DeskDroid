#pragma once

#include <Arduino.h>

class AnimationRuntime {
 public:
  void begin(uint16_t frameIntervalMs);
  bool shouldRender();

  uint32_t frameCount() const;

 private:
  uint16_t frameIntervalMs_ = 16;
  uint32_t lastFrameMs_ = 0;
  uint32_t frameCount_ = 0;
};
