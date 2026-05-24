#pragma once

#include <Arduino.h>

#include "hardware_types.h"
#include "settings_store.h"

struct RGBColor {
  RGBColor() = default;
  RGBColor(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

enum class EffectType : uint8_t {
  None,
  Solid,
  Breathing,
  Rainbow,
};

struct SystemState {
  static constexpr uint8_t RelayCount = 4;

  bool relayStates[RelayCount] = {};
  bool ledsEnabled = true;
  uint8_t brightness = 128;
  RGBColor currentColor{30, 0, 20};
  EffectType currentEffect = EffectType::Solid;
};

namespace SystemStateStore {

void begin(const DeviceSettings &settings);
const SystemState &current();

bool setRelay(uint8_t relayNumber, bool enabled);
void setLedsEnabled(bool enabled);
void setBrightnessLevel(uint8_t level);
void setIdlePreset(LedIdlePreset preset);
void setLedMode(LedState mode);

uint32_t revision();

}  // namespace SystemStateStore
