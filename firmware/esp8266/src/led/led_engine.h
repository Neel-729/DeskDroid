#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "animation_runtime.h"
#include "led_effects.h"
#include "../state/state_cache.h"

class LedEngine {
 public:
  explicit LedEngine(StateCache& state);

  void begin();
  void update();

  void clear();
  void setSolidColor(uint8_t r, uint8_t g, uint8_t b);
  void setBrightness(uint8_t brightness);
  void setEffect(LedEffect effect);
  void applyState();

 private:
  Effect* selectEffect(LedEffect effect);

  StateCache& state_;
  Adafruit_NeoPixel pixels_;
  AnimationRuntime frameScheduler_;
  SolidColorEffect solidEffect_;
  BreathingEffect breathingEffect_;
  RainbowEffect rainbowEffect_;
  Effect* activeEffect_ = nullptr;
  LedEffect activeEffectId_ = LedEffect::None;
};
