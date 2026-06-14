#include "application_commands.h"

#include <WiFi.h>

#include "../core/events.h"
#include "../core/logging.h"
#include "../core/persistent_storage.h"
#include "../core/settings_store.h"
#include "../core/time_service.h"
#include "../features/reminders.h"

namespace {

bool validClockFields(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second){
  return year >= 2000 &&
    year <= 2099 &&
    month >= 1 &&
    month <= 12 &&
    day >= 1 &&
    day <= 31 &&
    hour <= 23 &&
    minute <= 59 &&
    second <= 59;
}

}  // namespace

namespace AppCommands {

bool setBrightness(uint8_t value){
  SystemStateStore::setBrightness(value);
  enqueueEvent(EVENT_BRIGHTNESS_CHANGED, EventSource::APP);
  return true;
}

bool setBrightnessLevel(uint8_t level){
  if(level > 10) return false;
  SystemStateStore::setBrightnessLevel(level);
  enqueueEvent(EVENT_BRIGHTNESS_CHANGED, EventSource::APP);
  return true;
}

bool setAnimationMode(AnimationMode mode){
  SystemStateStore::setAnimationMode(mode);
  enqueueEvent(EVENT_MODE_CHANGED, EventSource::APP);
  return true;
}

bool setColor(RGBColor color){
  SystemStateStore::setColor(color);
  return true;
}

bool setLedMode(LedState mode){
  SystemStateStore::setLedMode(mode);
  enqueueEvent(EVENT_MODE_CHANGED, EventSource::APP);
  return true;
}

bool setIdlePreset(LedIdlePreset preset){
  if(preset > IDLE_AMBIENT) return false;
  SystemStateStore::setIdlePreset(preset);
  return true;
}

bool setLightingEnabled(bool enabled){
  SystemStateStore::setLightingEnabled(enabled);
  return true;
}

bool setRelay(uint8_t relayNumber, bool enabled){
  // Get current relay states before change
  bool relayStatesBefore[SystemState::RelayCount];
  SystemStateStore::getRelayStates(relayStatesBefore, SystemState::RelayCount);
  
  // Attempt to change the relay state
  if(!SystemStateStore::setRelay(relayNumber, enabled)){
    return false;
  }
  
  // Get relay states after change
  bool relayStatesAfter[SystemState::RelayCount];
  SystemStateStore::getRelayStates(relayStatesAfter, SystemState::RelayCount);
  
  // Check if state actually changed
  bool changed = false;
  for(uint8_t i = 0; i < SystemState::RelayCount; i++){
    if(relayStatesBefore[i] != relayStatesAfter[i]){
      changed = true;
      break;
    }
  }
  
  // Only persist if state actually changed
  if(changed){
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] Relay %d changed to %s", relayNumber, enabled ? "ON" : "OFF");
    PersistentStorage::saveRelayStates(relayStatesAfter, SystemState::RelayCount);
  }
  
  return true;
}

bool startTimer(uint32_t nowMs){
  SystemStateStore::startTimer(nowMs);
  enqueueTimerEvent(EVENT_TIMER_STARTED);
  return true;
}

bool pauseTimer(uint32_t nowMs){
  SystemStateStore::pauseTimer(nowMs);
  enqueueTimerEvent(EVENT_TIMER_STOPPED);
  return true;
}

bool stopTimer(){
  SystemStateStore::resetTimer();
  enqueueTimerEvent(EVENT_TIMER_STOPPED);
  return true;
}

bool resetTimer(){
  SystemStateStore::resetTimer();
  return true;
}

bool setTimerDuration(uint32_t durationMs){
  if(durationMs == 0 || durationMs > 99UL * 3600UL * 1000UL + 59UL * 60UL * 1000UL + 59UL * 1000UL){
    return false;
  }
  SystemStateStore::setTimerDuration(durationMs);
  return true;
}

bool adjustTimerEdit(int step){
  const TimerState &timer = SystemStateStore::current().timer;
  uint8_t h = timer.hours;
  uint8_t m = timer.minutes;
  uint8_t s = timer.seconds;

  if(timer.editField == EDIT_HOURS){
    int next = static_cast<int>(h) + step;
    if(next < 0) next = 0;
    if(next > 99) next = 99;
    h = static_cast<uint8_t>(next);
  } else if(timer.editField == EDIT_MINUTES){
    int next = static_cast<int>(m) + step;
    if(next < 0) next = 0;
    if(next > 59) next = 59;
    m = static_cast<uint8_t>(next);
  } else {
    int next = static_cast<int>(s) + step;
    if(next < 0) next = 0;
    if(next > 59) next = 59;
    s = static_cast<uint8_t>(next);
  }

  SystemStateStore::setTimerClockFields(h, m, s);
  return true;
}

bool advanceTimerEditField(){
  const TimerEditField field = SystemStateStore::current().timer.editField;
  if(field == EDIT_HOURS) SystemStateStore::setTimerEditField(EDIT_MINUTES);
  else if(field == EDIT_MINUTES) SystemStateStore::setTimerEditField(EDIT_SECONDS);
  else SystemStateStore::setTimerEditField(EDIT_HOURS);
  return true;
}

bool setReminder(uint8_t index, uint8_t hour, uint8_t minute, bool active){
  if(!RemindersFeature::setReminder(index, hour, minute, active)) return false;
  RemindersFeature::save();
  return true;
}

bool syncClock(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second){
  if(!validClockFields(year, month, day, hour, minute, second)) return false;
  TimeService::adjust(DateTime(year, month, day, hour, minute, second));
  return true;
}

bool setVolume(uint8_t volume){
  SystemStateStore::setAudioVolume(volume);
  return true;
}

bool setMuted(bool muted){
  SystemStateStore::setAudioMuted(muted);
  return true;
}

bool connectWifi(const char* ssid, const char* password){
  if(ssid == nullptr || ssid[0] == '\0' || password == nullptr) return false;
  WiFi.begin(ssid, password);
  return true;
}

bool applySettings(const DeviceSettings &settings, bool dirty){
  SystemStateStore::applySettings(settings, dirty);
  SystemStateStore::setBuzzerEnabled(settings.buzzer);
  SystemStateStore::setBrightnessLevel(settings.ledBrightness);
  SystemStateStore::setIdlePreset(static_cast<LedIdlePreset>(settings.idlePreset));
  enqueueEvent(EVENT_SETTINGS_LOADED, EventSource::APP);
  return true;
}

bool saveSettings(const DeviceSettings &settings){
  SettingsStore::saveDeviceSettings(settings);
  SystemStateStore::applySettings(settings, false);
  SystemStateStore::markSettingsDirty(false);
  return true;
}

}  // namespace AppCommands