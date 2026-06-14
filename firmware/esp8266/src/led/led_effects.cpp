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
  phase_ += 0.01F;
  if (phase_ > TWO_PI) {
    phase_ -= TWO_PI;
  }

  // Very slow color transition through warm ambient tones
  colorPhase_ += 0.0025F;
  if (colorPhase_ > TWO_PI) {
    colorPhase_ -= TWO_PI;
  }

  // Calculate brightness with a gentle sine wave, ensuring it's perceptually linear
  const float brightnessWave = (sin(phase_) + 1.0F) * 0.5F;
  const float minBrightness = 0.15F;
  const uint8_t brightness = static_cast<uint8_t>(255.0F * (minBrightness + ((1.0F - minBrightness) * brightnessWave)));

  // Animate Hue to cycle through warm colors (e.g., orange to yellow)
  // Hue is a value from 0-65535. Let's sweep a small range.
  // 2000 (~orange) to 8000 (~yellow)
  const float hueWave = (sin(colorPhase_) + 1.0F) * 0.5F;
  const uint16_t hue = 2000 + static_cast<uint16_t>(6000.0F * hueWave);

  // Keep saturation high for rich color
  const uint8_t saturation = 255;

  // Apply gamma correction for perceptually linear brightness
  const uint32_t color = pixels_.gamma32(pixels_.ColorHSV(hue, saturation, brightness));

  pixels_.fill(color);
  pixels_.show();
}