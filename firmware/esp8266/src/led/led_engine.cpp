#include "led_engine.h"

#include "led_effects.h"

LedEngine::LedEngine(StateCache& state)
    : state_(state),
      pixels_(Config::NeoPixelCount, Pins::NeoPixelData, NEO_GRB + NEO_KHZ800) {}

void LedEngine::begin() {
  pixels_.begin();
  pixels_.setBrightness(state_.brightness());
  pixels_.clear();
  pixels_.show();
}

void LedEngine::update() {
  // Placeholder for future effects. Current milestone only applies direct commands.
}

void LedEngine::clear() {
  LedEffects::clear(pixels_);
  pixels_.show();
  state_.setColor(0, 0, 0);
  state_.setActiveEffect(LedEffect::None);
}

void LedEngine::setSolidColor(uint8_t r, uint8_t g, uint8_t b) {
  LedEffects::solid(pixels_, r, g, b);
  pixels_.show();
  state_.setColor(r, g, b);
  state_.setActiveEffect(LedEffect::None);
}

void LedEngine::setBrightness(uint8_t brightness) {
  pixels_.setBrightness(brightness);
  state_.setBrightness(brightness);
  pixels_.show();
}
