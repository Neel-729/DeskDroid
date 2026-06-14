#include "led_engine.h"

#include <string.h>

LedEngine::LedEngine(StateCache& state)
    : state_(state),
      pixels_(Config::NeoPixelCount, Pins::NeoPixelData, NEO_GRB + NEO_KHZ800),
      solidEffect_(pixels_, state_),
      breathingEffect_(pixels_, state_),
      rainbowEffect_(pixels_),
      ambientEffect_(pixels_, state_) {}

void LedEngine::begin() {
  pixels_.begin();
  pixels_.setBrightness(state_.brightness());
  pixels_.clear();
  pixels_.show();
  frameScheduler_.begin(Config::LedFrameIntervalMs);
  effectStartedMs_ = millis();
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
  effectStartedMs_ = millis();
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

bool LedEngine::applyLedState(LedEffect effect, uint8_t brightness, uint8_t speed, bool enabled, RgbColor color) {
  const bool requestedEnabled = enabled && effect != LedEffect::None;
  const RgbColor currentColor = state_.color();
  const bool duplicate =
      state_.ledsEnabled() == requestedEnabled &&
      state_.activeEffect() == effect &&
      state_.brightness() == brightness &&
      speed_ == speed &&
      currentColor.r == color.r &&
      currentColor.g == color.g &&
      currentColor.b == color.b;

  if (duplicate) {
    diagnostics_.duplicateIgnored++;
    return false;
  }

  const bool wasEnabled = state_.ledsEnabled();
  const LedEffect previousEffect = state_.activeEffect();
  const bool colorChanged =
      currentColor.r != color.r || currentColor.g != color.g || currentColor.b != color.b;
  const bool brightnessChanged = state_.brightness() != brightness;
  const bool speedChanged = speed_ != speed;

  state_.setBrightness(brightness);
  state_.setColor(color.r, color.g, color.b);
  state_.setActiveEffect(effect);
  state_.setLedsEnabled(requestedEnabled);
  speed_ = speed;
  pixels_.setBrightness(brightness);

  if (!state_.ledsEnabled()) {
    if (activeEffect_ != nullptr || activeEffectId_ != LedEffect::None || wasEnabled) {
      activeEffect_ = nullptr;
      activeEffectId_ = LedEffect::None;
      pixels_.clear();
      pixels_.show();
      effectStartedMs_ = millis();
    }
    return true;
  }

  Effect* nextEffect = selectEffect(effect);
  const bool effectChanged = !wasEnabled || previousEffect != effect || activeEffect_ != nextEffect;
  activeEffect_ = nextEffect;
  activeEffectId_ = effect;

  if (activeEffect_ == nullptr) {
    pixels_.clear();
    pixels_.show();
    effectStartedMs_ = millis();
    return true;
  }

  if (effectChanged) {
    activeEffect_->begin();
    effectStartedMs_ = millis();
    return true;
  }

  if (colorChanged || brightnessChanged || speedChanged) {
    if (effect == LedEffect::Solid) {
      activeEffect_->begin();
    } else {
      activeEffect_->update();
    }
  }

  return true;
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
    effectStartedMs_ = millis();
  }
}

void LedEngine::recordCommandResult(const char* command, const char* result, const char* reason) {
  diagnostics_.lastCommandMs = millis();
  copyText(diagnostics_.lastCommand, sizeof(diagnostics_.lastCommand), command);
  copyText(diagnostics_.lastResult, sizeof(diagnostics_.lastResult), result);
  copyText(diagnostics_.lastReason, sizeof(diagnostics_.lastReason), reason);
}

const LedEngineDiagnostics& LedEngine::diagnostics() const {
  LedEngine* self = const_cast<LedEngine*>(this);
  self->diagnostics_.activeEffect = activeEffectId_;
  self->diagnostics_.enabled = state_.ledsEnabled();
  self->diagnostics_.brightness = state_.brightness();
  self->diagnostics_.speed = speed_;
  self->diagnostics_.animationRuntimeMs =
      activeEffect_ == nullptr ? 0 : millis() - effectStartedMs_;
  return diagnostics_;
}

const char* LedEngine::effectName(LedEffect effect) {
  switch (effect) {
    case LedEffect::None:
      return "NONE";
    case LedEffect::Solid:
      return "SOLID";
    case LedEffect::Breathing:
      return "BREATHING";
    case LedEffect::Rainbow:
      return "RAINBOW";
    case LedEffect::Ambient:
      return "AMBIENT";
  }
  return "NONE";
}

Effect* LedEngine::selectEffect(LedEffect effect) {
  switch (effect) {
    case LedEffect::Solid:
      return &solidEffect_;
    case LedEffect::Breathing:
      return &breathingEffect_;
    case LedEffect::Rainbow:
      return &rainbowEffect_;
    case LedEffect::Ambient:
      return &ambientEffect_;
    case LedEffect::None:
      return nullptr;
  }

  return nullptr;
}

void LedEngine::copyText(char* destination, size_t destinationSize, const char* source) {
  if (destination == nullptr || destinationSize == 0) {
    return;
  }
  if (source == nullptr) {
    source = "";
  }
  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}