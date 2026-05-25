#pragma once

#include <Arduino.h>

#include "../core/system_state.h"

namespace PacketBuilder {

size_t ping(char* buffer, size_t bufferSize, uint16_t sequenceId = 0);
size_t setRelay(char* buffer, size_t bufferSize, uint8_t relayNumber, bool enabled);
size_t setColor(char* buffer, size_t bufferSize, const RGBColor &color);
size_t setBrightness(char* buffer, size_t bufferSize, uint8_t brightness);
size_t setEffect(char* buffer, size_t bufferSize, EffectType effect);
size_t fullSync(char* buffer, size_t bufferSize, const SystemState &state, uint16_t sequenceId = 0);
const char* effectName(EffectType effect);

}  // namespace PacketBuilder
