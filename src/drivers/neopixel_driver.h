#pragma once

#include <Arduino.h>

enum LedState {
  LED_IDLE,
  LED_TIMER_ALARM,
  LED_REMINDER_ALARM,
  LED_SUCCESS
};

enum LedIdlePreset {
  IDLE_OFF,
  IDLE_STATIC,
  IDLE_BREATH,
  IDLE_RAINBOW,
  IDLE_PULSE
};

namespace NeoPixelDriver {
void begin(uint8_t brightnessLevel, uint8_t idlePreset);
void update(bool lightsAllowed);
void setState(LedState state);
void setIdlePreset(LedIdlePreset preset);
void setBrightnessLevel(uint8_t level);
}

