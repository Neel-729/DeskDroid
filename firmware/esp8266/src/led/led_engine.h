#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "animation_runtime.h"
#include "led_effects.h"
#include "../state/state_cache.h"

struct LedEngineDiagnostics {
  LedEffect activeEffect = LedEffect::None;
  bool enabled = false;
  uint8_t brightness = 0;
  uint8_t speed = 0;
  uint32_t animationRuntimeMs = 0;
  uint32_t duplicateIgnored = 0;
  uint32_t lastCommandMs = 0;
  char lastCommand[16] = "";
  char lastResult[16] = "";
  char lastReason[32] = "";
};

class LedEngine {
 public:
  explicit LedEngine(StateCache& state);

  void begin();
  void update();

  void clear();
  void setSolidColor(uint8_t r, uint8_t g, uint8_t b);
  void setBrightness(uint8_t brightness);
  void setEffect(LedEffect effect);
  bool applyLedState(LedEffect effect, uint8_t brightness, uint8_t speed, bool enabled, RgbColor color);
  void applyState();
  void recordCommandResult(const char* command, const char* result, const char* reason);
  const LedEngineDiagnostics& diagnostics() const;
  static const char* effectName(LedEffect effect);

 private:
  Effect* selectEffect(LedEffect effect);
  void copyText(char* destination, size_t destinationSize, const char* source);

  StateCache& state_;
  Adafruit_NeoPixel pixels_;
  AnimationRuntime frameScheduler_;
  SolidColorEffect solidEffect_;
  BreathingEffect breathingEffect_;
  RainbowEffect rainbowEffect_;
  AmbientEffect ambientEffect_;
  Effect* activeEffect_ = nullptr;
  LedEffect activeEffectId_ = LedEffect::None;
  uint8_t speed_ = 5;
  uint32_t effectStartedMs_ = 0;
  LedEngineDiagnostics diagnostics_;
};