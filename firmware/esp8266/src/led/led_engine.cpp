#include "led_engine.h"

LedEngine::LedEngine(StateCache& state)
    : state_(state),
      pixels_(Config::NeoPixelCount, Pins::NeoPixelData, NEO_GRB + NEO_KHZ800),
      solidEffect_(pixels_, state_),
      breathingEffect_(pixels_, state_),
      rainbowEffect_(pixels_) {}

void LedEngine::begin() {
  pixels_.begin();
  pixels_.setBrightness(state_.brightness());
  pixels_.clear();
  pixels_.show();
  frameScheduler_.begin(Config::LedFrameIntervalMs);
}

void LedEngine::update() {
  if (activeEffect_ == nullptr || !frameScheduler_.shouldRender()) {
    return;
  }

  activeEffect_->update();
}

void LedEngine::clear() {
  activeEffect_ = nullptr;
  activeEffectId_ = LedEffect::None;
  pixels_.clear();
  pixels_.show();
  state_.setColor(0, 0, 0);
  state_.setActiveEffect(LedEffect::None);
  state_.setLedsEnabled(false);
}

void LedEngine::setSolidColor(uint8_t r, uint8_t g, uint8_t b) {
  state_.setColor(r, g, b);
  state_.setActiveEffect(LedEffect::Solid);
  state_.setLedsEnabled(true);
  applyState();
}

void LedEngine::setBrightness(uint8_t brightness) {
  pixels_.setBrightness(brightness);
  state_.setBrightness(brightness);
  pixels_.show();
}

void LedEngine::setEffect(LedEffect effect) {
  state_.setActiveEffect(effect);
  state_.setLedsEnabled(effect != LedEffect::None);
  applyState();
}

void LedEngine::applyState() {
  pixels_.setBrightness(state_.brightness());

  if (!state_.ledsEnabled() || state_.activeEffect() == LedEffect::None) {
    activeEffect_ = nullptr;
    activeEffectId_ = LedEffect::None;
    pixels_.clear();
    pixels_.show();
    return;
  }

  activeEffect_ = selectEffect(state_.activeEffect());
  activeEffectId_ = state_.activeEffect();
  if (activeEffect_ != nullptr) {
    activeEffect_->begin();
  }
}

Effect* LedEngine::selectEffect(LedEffect effect) {
  switch (effect) {
    case LedEffect::Solid:
      return &solidEffect_;
    case LedEffect::Breathing:
      return &breathingEffect_;
    case LedEffect::Rainbow:
      return &rainbowEffect_;
    case LedEffect::None:
      return nullptr;
  }

  return nullptr;
}
