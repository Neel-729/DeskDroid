#pragma once

#include <Arduino.h>

enum LedState : uint8_t {
  LED_IDLE,
  LED_TIMER_ALARM,
  LED_REMINDER_ALARM,
  LED_SUCCESS
};

enum LedIdlePreset : uint8_t {
  IDLE_OFF,
  IDLE_STATIC,
  IDLE_BREATH,
  IDLE_RAINBOW,
  IDLE_PULSE,
  IDLE_AMBIENT
};