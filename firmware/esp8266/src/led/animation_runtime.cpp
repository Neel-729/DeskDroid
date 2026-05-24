#include "animation_runtime.h"

void AnimationRuntime::begin() {
  frameCount_ = 0;
}

void AnimationRuntime::update() {
  ++frameCount_;
}

uint32_t AnimationRuntime::frameCount() const {
  return frameCount_;
}

