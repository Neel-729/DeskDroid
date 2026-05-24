#include "led_effects.h"

namespace LedEffects {

void clear(Adafruit_NeoPixel& pixels) {
  pixels.clear();
}

void solid(Adafruit_NeoPixel& pixels, uint8_t r, uint8_t g, uint8_t b) {
  pixels.fill(pixels.Color(r, g, b));
}

}  // namespace LedEffects

