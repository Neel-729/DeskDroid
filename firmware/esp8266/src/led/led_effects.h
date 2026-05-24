#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "../state/state_cache.h"

class Effect {
 public:
  virtual ~Effect() = default;
  virtual void begin() = 0;
  virtual void update() = 0;
};

class SolidColorEffect : public Effect {
 public:
  SolidColorEffect(Adafruit_NeoPixel& pixels, const StateCache& state);

  void begin() override;
  void update() override;

 private:
  void render();

  Adafruit_NeoPixel& pixels_;
  const StateCache& state_;
};

class BreathingEffect : public Effect {
 public:
  BreathingEffect(Adafruit_NeoPixel& pixels, const StateCache& state);

  void begin() override;
  void update() override;

 private:
  Adafruit_NeoPixel& pixels_;
  const StateCache& state_;
  float phase_ = 0.0F;
};

class RainbowEffect : public Effect {
 public:
  explicit RainbowEffect(Adafruit_NeoPixel& pixels);

  void begin() override;
  void update() override;

 private:
  Adafruit_NeoPixel& pixels_;
  uint16_t hue_ = 0;
};
