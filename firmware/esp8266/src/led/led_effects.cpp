#include "led_effects.h"

#include <math.h>

SolidColorEffect::SolidColorEffect(Adafruit_NeoPixel& pixels, const StateCache& state)
    : pixels_(pixels), state_(state) {}

void SolidColorEffect::begin() {
  render();
}

void SolidColorEffect::update() {}

void SolidColorEffect::render() {
  const RgbColor color = state_.color();
  pixels_.fill(pixels_.Color(color.r, color.g, color.b));
  pixels_.show();
}

BreathingEffect::BreathingEffect(Adafruit_NeoPixel& pixels, const StateCache& state)
    : pixels_(pixels), state_(state) {}

void BreathingEffect::begin() {
  phase_ = 0.0F;
  update();
}

void BreathingEffect::update() {
  phase_ += 0.05F;
  if (phase_ > TWO_PI) {
    phase_ -= TWO_PI;
  }

  const float wave = (sin(phase_) + 1.0F) * 0.5F;
  const float floor = 0.12F;
  const float level = floor + ((1.0F - floor) * wave);
  const RgbColor color = state_.color();

  pixels_.fill(pixels_.Color(static_cast<uint8_t>(color.r * level),
                             static_cast<uint8_t>(color.g * level),
                             static_cast<uint8_t>(color.b * level)));
  pixels_.show();
}

RainbowEffect::RainbowEffect(Adafruit_NeoPixel& pixels) : pixels_(pixels) {}

void RainbowEffect::begin() {
  hue_ = 0;
  update();
}

void RainbowEffect::update() {
  hue_ += 256;

  for (uint16_t i = 0; i < pixels_.numPixels(); ++i) {
    const uint16_t pixelHue = hue_ + (i * 65536UL / pixels_.numPixels());
    pixels_.setPixelColor(i, pixels_.gamma32(pixels_.ColorHSV(pixelHue)));
  }

  pixels_.show();
}

AmbientEffect::AmbientEffect(Adafruit_NeoPixel& pixels, const StateCache& state)
    : pixels_(pixels), state_(state) {}

void AmbientEffect::begin() {
  phase_ = 0.0F;
  colorPhase_ = 0.0F;
  update();
}

void AmbientEffect::update() {
  // Slow brightness breathing
  phase_ += 0.02F;
  if (phase_ > TWO_PI) {
    phase_ -= TWO_PI;
  }
  
  // Very slow color transition through warm ambient tones
  colorPhase_ += 0.005F;
  if (colorPhase_ > TWO_PI) {
    colorPhase_ -= TWO_PI;
  }
  
  const float brightnessWave = (sin(phase_) + 1.0F) * 0.5F;
  const float floor = 0.15F;
  const float brightness = floor + ((1.0F - floor) * brightnessWave);
  
  // Cycle through warm colors: warm white -> soft orange -> warm amber -> back
  const float colorT = (sin(colorPhase_) + 1.0F) * 0.5F;
  
  // Interpolate between warm ambient colors
  // Base warm white: (60, 40, 25) -> Soft orange: (80, 45, 20) -> Warm amber: (70, 50, 15)
  uint8_t r = static_cast<uint8_t>((60 + (20 * sin(colorPhase_))) * brightness);
  uint8_t g = static_cast<uint8_t>((40 + (10 * cos(colorPhase_))) * brightness);
  uint8_t b = static_cast<uint8_t>((25 - (10 * sin(colorPhase_ * 0.5))) * brightness);
  
  pixels_.fill(pixels_.Color(r, g, b));
  pixels_.show();
}

AmbientEffect::AmbientEffect(Adafruit_NeoPixel& pixels, const StateCache& state)
    : pixels_(pixels), state_(state) {}

void AmbientEffect::begin() {
  phase_ = 0.0F;
  colorPhase_ = 0.0F;
  update();
}

void AmbientEffect::update() {
  // Slow brightness breathing
  phase_ += 0.02F;
  if (phase_ > TWO_PI) {
    phase_ -= TWO_PI;
  }
  
  // Very slow color transition through warm ambient tones
  colorPhase_ += 0.005F;
  if (colorPhase_ > TWO_PI) {
    colorPhase_ -= TWO_PI;
  }
  
  const float brightnessWave = (sin(phase_) + 1.0F) * 0.5F;
  const float floor = 0.15F;
  const float brightness = floor + ((1.0F - floor) * brightnessWave);
  
  // Cycle through warm colors: warm white -> soft orange -> warm amber -> back
  const float colorT = (sin(colorPhase_) + 1.0F) * 0.5F;
  
  // Interpolate between warm ambient colors
  // Base warm white: (60, 40, 25) -> Soft orange: (80, 45, 20) -> Warm amber: (70, 50, 15)
  uint8_t r = static_cast<uint8_t>((60 + (20 * sin(colorPhase_))) * brightness);
  uint8_t g = static_cast<uint8_t>((40 + (10 * cos(colorPhase_))) * brightness);
  uint8_t b = static_cast<uint8_t>((25 - (10 * sin(colorPhase_ * 0.5))) * brightness);
  
  pixels_.fill(pixels_.Color(r, g, b));
  pixels_.show();
}

AmbientEffect::AmbientEffect(Adafruit_NeoPixel& pixels, const StateCache& state)
    : pixels_(pixels), state_(state) {}

void AmbientEffect::begin() {
  phase_ = 0.0F;
  colorPhase_ = 0.0F;
  update();
}

void AmbientEffect::update() {
  // Slow brightness breathing
  phase_ += 0.02F;
  if (phase_ > TWO_PI) {
    phase_ -= TWO_PI;
  }
  
  // Very slow color transition through warm ambient tones
  colorPhase_ += 0.005F;
  if (colorPhase_ > TWO_PI) {
    colorPhase_ -= TWO_PI;
  }
  
  const float brightnessWave = (sin(phase_) + 1.0F) * 0.5F;
  const float floor = 0.15F;
  const float brightness = floor + ((1.0F - floor) * brightnessWave);
  
  // Cycle through warm colors: warm white -> soft orange -> warm amber -> back
  const float colorT = (sin(colorPhase_) + 1.0F) * 0.5F;
  
  // Interpolate between warm ambient colors
  // Base warm white: (60, 40, 25) -> Soft orange: (80, 45, 20) -> Warm amber: (70, 50, 15)
  uint8_t r = static_cast<uint8_t>((60 + (20 * sin(colorPhase_))) * brightness);
  uint8_t g = static_cast<uint8_t>((40 + (10 * cos(colorPhase_))) * brightness);
  uint8_t b = static_cast<uint8_t>((25 - (10 * sin(colorPhase_ * 0.5))) * brightness);
  
  pixels_.fill(pixels_.Color(r, g, b));
  pixels_.show();
}