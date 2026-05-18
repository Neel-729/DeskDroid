#pragma once

#include <Arduino.h>

#include "../core/hardware_types.h"
#include "../core/settings_store.h"

enum class HardwareCommandType : uint8_t {
  BEEP,
  LED_MODE,
  IDLE_PRESET,
  LED_BRIGHTNESS,
  BACKLIGHT
};

enum class CommandSource : uint8_t {
  SYSTEM,
  APP,
  UI,
  TIMER,
  REMINDER,
  SETTINGS,
  LIGHTING,
  INPUTS
};

struct HardwareCommand {
  HardwareCommandType type;
  uint16_t value;
  uint32_t timestamp;
  uint16_t sequenceId;
  CommandSource source;
};

struct HardwareRequestStats {
  uint32_t queued;
  uint32_t executed;
  uint16_t dropped;
  uint8_t maxDepth;
  uint16_t lastSequenceId;
  uint32_t maxCommandAgeMs;
};

namespace HardwareRequests {
void beginLocal(const DeviceSettings &settings);

void requestBeep(uint16_t durationMs, CommandSource source = CommandSource::APP);
void requestLedMode(LedState mode, CommandSource source = CommandSource::APP);
void requestIdlePreset(LedIdlePreset preset, CommandSource source = CommandSource::APP);
void requestLedBrightness(uint8_t level, CommandSource source = CommandSource::APP);
void requestBacklight(bool enabled, CommandSource source = CommandSource::APP);

bool enqueue(const HardwareCommand &command);
void executePending();
const HardwareRequestStats &stats();

void serviceBuzzer();
void updateLeds(bool lightsAllowed);
void clearDisplay();
void writeDisplayRows(const char* row0, const char* row1);
}
