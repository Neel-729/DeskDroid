#include "packet_builder.h"

#include <stdarg.h>
#include <stdio.h>

namespace PacketBuilder {

namespace {

size_t checkedSnprintf(char* buffer, size_t bufferSize, const char* format, ...){
  if(buffer == nullptr || bufferSize == 0) {
    return 0;
  }

  va_list args;
  va_start(args, format);
  const int written = vsnprintf(buffer, bufferSize, format, args);
  va_end(args);

  if(written <= 0 || static_cast<size_t>(written) >= bufferSize){
    buffer[0] = '\0';
    return 0;
  }

  return static_cast<size_t>(written);
}

}  // namespace

size_t ping(char* buffer, size_t bufferSize, uint16_t sequenceId) {
  if(sequenceId == 0) return checkedSnprintf(buffer, bufferSize, "<PING>");
  return checkedSnprintf(buffer, bufferSize, "<PING|SEQ=%u>", sequenceId);
}

size_t setRelay(char* buffer, size_t bufferSize, uint8_t relayNumber, bool enabled) {
  return checkedSnprintf(buffer, bufferSize, "<SET_RELAY|%u|%s>", relayNumber, enabled ? "ON" : "OFF");
}

size_t setColor(char* buffer, size_t bufferSize, const RGBColor &color) {
  return checkedSnprintf(buffer, bufferSize, "<SET_COLOR|%u|%u|%u>", color.r, color.g, color.b);
}

size_t setBrightness(char* buffer, size_t bufferSize, uint8_t brightness) {
  return checkedSnprintf(buffer, bufferSize, "<SET_BRIGHTNESS|%u>", brightness);
}

size_t setEffect(char* buffer, size_t bufferSize, EffectType effect) {
  return checkedSnprintf(buffer, bufferSize, "<SET_EFFECT|%s>", effectName(effect));
}

size_t setLedState(char* buffer, size_t bufferSize, const LightingState &state, uint8_t speed, uint16_t sequenceId) {
  return checkedSnprintf(
    buffer,
    bufferSize,
    "<SET_LED_STATE|SEQ=%u|MODE=%s|BR=%u|SPD=%u|PWR=%u|CR=%u,%u,%u>",
    sequenceId,
    effectName(state.mode),
    state.brightness,
    speed,
    state.enabled && state.scheduleAllowsOutput ? 1 : 0,
    state.color.r,
    state.color.g,
    state.color.b
  );
}

size_t fullSync(char* buffer, size_t bufferSize, const SystemState &state, uint16_t sequenceId) {
  return checkedSnprintf(
    buffer,
    bufferSize,
    "<FULL_SYNC|SEQ=%u|R1=%u|R2=%u|R3=%u|R4=%u>",
    sequenceId,
    state.relayStates[0] ? 1 : 0,
    state.relayStates[1] ? 1 : 0,
    state.relayStates[2] ? 1 : 0,
    state.relayStates[3] ? 1 : 0
  );
}

const char* effectName(EffectType effect) {
  switch(effect){
    case EffectType::None: return "NONE";
    case EffectType::Solid: return "SOLID";
    case EffectType::Breathing: return "BREATHING";
    case EffectType::Rainbow: return "RAINBOW";
    case EffectType::Ambient: return "AMBIENT";
  }

  return "NONE";}

}  // namespace PacketBuilder