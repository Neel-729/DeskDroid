#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

namespace LedEffects {

void clear(Adafruit_NeoPixel& pixels);
void solid(Adafruit_NeoPixel& pixels, uint8_t r, uint8_t g, uint8_t b);

}  // namespace LedEffects

