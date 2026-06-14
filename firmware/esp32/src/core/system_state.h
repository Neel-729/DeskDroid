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
  Ambient,
};

using AnimationMode = EffectType;

enum TimerEditField : uint8_t {
  EDIT_HOURS,
  EDIT_MINUTES,
  EDIT_SECONDS
};

enum class StateChange : uint16_t {
  None = 0,
  Lighting = 1 << 0,
  Timer = 1 << 1,
  Connectivity = 1 << 2,
  Audio = 1 << 3,
  Settings = 1 << 4,
  Protocol = 1 << 5,
};

struct LightingState {
  bool enabled = true;
  bool scheduleAllowsOutput = true;
  bool backlightEnabled = true;
  uint8_t brightness = 128;
  AnimationMode mode = AnimationMode::Solid;
  RGBColor color{30, 0, 20};
  LedState semanticMode = LED_IDLE;
  LedIdlePreset idlePreset = IDLE_STATIC;
};

struct TimerState {
  bool active = false;
  bool alarmActive = false;
  uint32_t durationMs = 5UL * 60UL * 1000UL;
  uint32_t remainingMs = 5UL * 60UL * 1000UL;
  uint32_t startedAtMs = 0;
  uint32_t endsAtMs = 0;
  uint8_t hours = 0;
  uint8_t minutes = 5;
  uint8_t seconds = 0;
  TimerEditField editField = EDIT_MINUTES;
  uint32_t alarmStartedAtMs = 0;
  uint32_t lastAlarmBeepMs = 0;
};

struct ConnectivityState {
  bool wifiConnected = false;
  bool esp8266Connected = false;
  int32_t rssi = 0;
};

struct AudioState {
  uint8_t volume = 128;
  bool muted = false;
  bool buzzerEnabled = true;
};

struct SettingsState {
  DeviceSettings device;
  bool loaded = false;
  bool dirty = false;
};

struct ProtocolState {
  bool esp8266Connected = false;
  uint32_t lastSyncRevision = 0;
  uint16_t lastSequenceId = 0;
};

struct SystemState {
  static constexpr uint8_t RelayCount = 4;

  bool relayStates[RelayCount] = {};
  LightingState lighting;
  TimerState timer;
  ConnectivityState connectivity;
  AudioState audio;
  SettingsState settings;
  ProtocolState protocol;
};

namespace SystemStateStore {

void begin(const DeviceSettings &settings);
const SystemState &current();

bool setRelay(uint8_t relayNumber, bool enabled);
void setLightingEnabled(bool enabled);
void setLightingScheduleAllowed(bool allowed);
void setBacklightEnabled(bool enabled);
void setBrightness(uint8_t brightness);
void setBrightnessLevel(uint8_t level);
void setColor(RGBColor color);
void setAnimationMode(AnimationMode mode);
void setIdlePreset(LedIdlePreset preset);
void setLedMode(LedState mode);
void applySettings(const DeviceSettings &settings, bool dirty = false);
void markSettingsDirty(bool dirty);

void setTimerDuration(uint32_t durationMs);
void setTimerClockFields(uint8_t hours, uint8_t minutes, uint8_t seconds);
void setTimerEditField(TimerEditField field);
void startTimer(uint32_t nowMs);
void pauseTimer(uint32_t nowMs);
void resetTimer();
void completeTimer(uint32_t nowMs);
void startTimerAlarm(uint32_t nowMs);
void stopTimerAlarm(bool restoreDuration);
void updateTimerRemaining(uint32_t nowMs);
void markTimerAlarmBeep(uint32_t nowMs);

void setWifiConnected(bool connected, int32_t rssi = 0);
void setEsp8266Connected(bool connected);
void setAudioVolume(uint8_t volume);
void setAudioMuted(bool muted);
void setBuzzerEnabled(bool enabled);
void markProtocolSynced(uint32_t revision, uint16_t sequenceId);

uint32_t revision();
uint16_t lastChangeMask();

}  // namespace SystemStateStore