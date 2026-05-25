#pragma once

#include <Arduino.h>

#include "../core/system_state.h"

namespace AppCommands {

bool setBrightness(uint8_t value);
bool setBrightnessLevel(uint8_t level);
bool setAnimationMode(AnimationMode mode);
bool setColor(RGBColor color);
bool setLedMode(LedState mode);
bool setIdlePreset(LedIdlePreset preset);
bool setLightingEnabled(bool enabled);
bool setRelay(uint8_t relayNumber, bool enabled);

bool startTimer(uint32_t nowMs);
bool pauseTimer(uint32_t nowMs);
bool stopTimer();
bool resetTimer();
bool setTimerDuration(uint32_t durationMs);
bool adjustTimerEdit(int step);
bool advanceTimerEditField();

bool setReminder(uint8_t index, uint8_t hour, uint8_t minute, bool active);
bool syncClock(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);

bool setVolume(uint8_t volume);
bool setMuted(bool muted);
bool connectWifi(const char* ssid, const char* password);

bool applySettings(const DeviceSettings &settings, bool dirty = true);
bool saveSettings(const DeviceSettings &settings);

}  // namespace AppCommands
