#pragma once

#include <Arduino.h>

#include "../core/hardware_types.h"

namespace NeoPixelDriver {
void begin(uint8_t brightnessLevel, uint8_t idlePreset);
void update(bool lightsAllowed);
void setState(LedState state);
void setIdlePreset(LedIdlePreset preset);
void setBrightnessLevel(uint8_t level);
}
