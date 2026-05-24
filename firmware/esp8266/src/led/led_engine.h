#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "../state/state_cache.h"

class LedEngine {
 public:
  explicit LedEngine(StateCache& state);

  void begin();
  void update();

  void clear();
  void setSolidColor(uint8_t r, uint8_t g, uint8_t b);
  void setBrightness(uint8_t brightness);

 private:
  StateCache& state_;
  Adafruit_NeoPixel pixels_;
};

