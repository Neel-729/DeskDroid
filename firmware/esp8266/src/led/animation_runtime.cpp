#include "animation_runtime.h"

void AnimationRuntime::begin(uint16_t frameIntervalMs) {
  frameIntervalMs_ = frameIntervalMs;
  lastFrameMs_ = millis();
  frameCount_ = 0;
}

bool AnimationRuntime::shouldRender() {
  const uint32_t now = millis();
  if (now - lastFrameMs_ < frameIntervalMs_) {
    return false;
  }

  lastFrameMs_ = now;
  ++frameCount_;
  return true;
}

uint32_t AnimationRuntime::frameCount() const {
  return frameCount_;
}
