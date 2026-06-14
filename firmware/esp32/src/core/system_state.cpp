#include "system_state.h"

#include "events.h"

namespace {

SystemState state;
uint32_t stateRevision = 0;
uint16_t lastMask = static_cast<uint16_t>(StateChange::None);

uint8_t brightnessLevelToByte(uint8_t level){
  if(level > 10) level = 10;
  return static_cast<uint8_t>(level * 25);
}

uint32_t timerDurationFromFields(uint8_t hours, uint8_t minutes, uint8_t seconds){
  uint32_t duration =
    (uint32_t)hours * 3600UL * 1000UL +
    (uint32_t)minutes * 60UL * 1000UL +
    (uint32_t)seconds * 1000UL;
  return duration == 0 ? 1000UL : duration;
}

void markChanged(StateChange change){
  stateRevision++;
  if(stateRevision == 0) stateRevision = 1;

  lastMask = static_cast<uint16_t>(change);
  enqueueStateChangedEvent(lastMask, stateRevision);
}

bool sameColor(const RGBColor &left, const RGBColor &right){
  return left.r == right.r && left.g == right.g && left.b == right.b;
}

void setLightingFromPreset(LedIdlePreset preset){
  state.lighting.idlePreset = preset;

  switch(preset){
    case IDLE_OFF:
      state.lighting.enabled = false;
      state.lighting.mode = AnimationMode::None;
      state.lighting.color = RGBColor(0, 0, 0);
      break;

    case IDLE_STATIC:
      state.lighting.enabled = true;
      state.lighting.mode = AnimationMode::Solid;
      state.lighting.color = RGBColor(30, 0, 20);
      break;

    case IDLE_BREATH:
    case IDLE_PULSE:
      state.lighting.enabled = true;
      state.lighting.mode = AnimationMode::Breathing;
      state.lighting.color = RGBColor(30, 0, 20);
      break;

    case IDLE_RAINBOW:
      state.lighting.enabled = true;
      state.lighting.mode = AnimationMode::Rainbow;
      state.lighting.color = RGBColor(30, 0, 20);
      break;
  }
}

void setLightingFromSemanticMode(LedState mode){
  state.lighting.semanticMode = mode;

  switch(mode){
    case LED_IDLE:
      setLightingFromPreset(state.lighting.idlePreset);
      break;

    case LED_TIMER_ALARM:
      state.lighting.enabled = true;
      state.lighting.mode = AnimationMode::Solid;
      state.lighting.color = RGBColor(255, 0, 0);
      break;

    case LED_REMINDER_ALARM:
      state.lighting.enabled = true;
      state.lighting.mode = AnimationMode::Solid;
      state.lighting.color = RGBColor(255, 80, 0);
      break;

    case LED_SUCCESS:
      state.lighting.enabled = true;
      state.lighting.mode = AnimationMode::Solid;
      state.lighting.color = RGBColor(0, 255, 0);
      break;
  }
}

bool lightingChanged(const LightingState &before){
  return before.enabled != state.lighting.enabled ||
    before.scheduleAllowsOutput != state.lighting.scheduleAllowsOutput ||
    before.backlightEnabled != state.lighting.backlightEnabled ||
    before.brightness != state.lighting.brightness ||
    before.mode != state.lighting.mode ||
    !sameColor(before.color, state.lighting.color) ||
    before.semanticMode != state.lighting.semanticMode ||
    before.idlePreset != state.lighting.idlePreset;
}

}  // namespace

namespace SystemStateStore {

void begin(const DeviceSettings &settings){
  state = {};
  applySettings(settings, false);
  state.lighting.brightness = brightnessLevelToByte(settings.ledBrightness);
  state.lighting.idlePreset = static_cast<LedIdlePreset>(settings.idlePreset);
  state.lighting.semanticMode = LED_IDLE;
  state.audio.buzzerEnabled = settings.buzzer;
  setLightingFromPreset(state.lighting.idlePreset);

  state.timer.durationMs = timerDurationFromFields(
    state.timer.hours,
    state.timer.minutes,
    state.timer.seconds
  );
  state.timer.remainingMs = state.timer.durationMs;
  markChanged(StateChange::Settings);
  markChanged(StateChange::Lighting);
  markChanged(StateChange::Timer);
}

const SystemState &current(){
  return state;
}

bool setRelay(uint8_t relayNumber, bool enabled){
  if(relayNumber == 0 || relayNumber > SystemState::RelayCount) return false;

  bool &target = state.relayStates[relayNumber - 1];
  if(target == enabled) return true;

  target = enabled;
  markChanged(StateChange::Protocol);
  return true;
}

void restoreRelayStates(const bool* states, uint8_t count){
  if(!states || count == 0 || count > SystemState::RelayCount) return;
  
  for(uint8_t i = 0; i < count; i++){
    state.relayStates[i] = states[i];
  }
  markChanged(StateChange::Protocol);
}

void getRelayStates(bool* states, uint8_t count){
  if(!states || count == 0 || count > SystemState::RelayCount) return;
  
  for(uint8_t i = 0; i < count; i++){
    states[i] = state.relayStates[i];
  }
}

void setLightingEnabled(bool enabled){
  if(state.lighting.enabled == enabled) return;
  state.lighting.enabled = enabled;
  markChanged(StateChange::Lighting);
}

void setLightingScheduleAllowed(bool allowed){
  if(state.lighting.scheduleAllowsOutput == allowed) return;
  state.lighting.scheduleAllowsOutput = allowed;
  markChanged(StateChange::Lighting);
}

void setBacklightEnabled(bool enabled){
  if(state.lighting.backlightEnabled == enabled) return;
  state.lighting.backlightEnabled = enabled;
  markChanged(StateChange::Lighting);
}

void setBrightness(uint8_t brightness){
  if(state.lighting.brightness == brightness) return;
  state.lighting.brightness = brightness;
  markChanged(StateChange::Lighting);
}

void setBrightnessLevel(uint8_t level){
  setBrightness(brightnessLevelToByte(level));
}

void setColor(RGBColor color){
  if(sameColor(state.lighting.color, color)) return;
  state.lighting.color = color;
  state.lighting.mode = AnimationMode::Solid;
  markChanged(StateChange::Lighting);
}

void setAnimationMode(AnimationMode mode){
  if(state.lighting.mode == mode) return;
  state.lighting.mode = mode;
  markChanged(StateChange::Lighting);
}

void setIdlePreset(LedIdlePreset preset){
  LightingState before = state.lighting;
  if(state.lighting.semanticMode != LED_IDLE){
    state.lighting.idlePreset = preset;
    if(lightingChanged(before)) markChanged(StateChange::Lighting);
    return;
  }
  setLightingFromPreset(preset);
  if(lightingChanged(before)) markChanged(StateChange::Lighting);
}

void setLedMode(LedState mode){
  LightingState before = state.lighting;
  setLightingFromSemanticMode(mode);
  if(lightingChanged(before)) markChanged(StateChange::Lighting);
}

void applySettings(const DeviceSettings &settings, bool dirty){
  const bool changed =
    !state.settings.loaded ||
    state.settings.dirty != dirty ||
    state.settings.device.buzzer != settings.buzzer ||
    state.settings.device.quotes != settings.quotes ||
    state.settings.device.format24 != settings.format24 ||
    state.settings.device.brightness != settings.brightness ||
    state.settings.device.ledBrightness != settings.ledBrightness ||
    state.settings.device.idlePreset != settings.idlePreset ||
    state.settings.device.autoLights != settings.autoLights ||
    state.settings.device.lightsOnHour != settings.lightsOnHour ||
    state.settings.device.lightsOnMinute != settings.lightsOnMinute ||
    state.settings.device.lightsOffHour != settings.lightsOffHour ||
    state.settings.device.lightsOffMinute != settings.lightsOffMinute;

  state.settings.device = settings;
  state.settings.loaded = true;
  state.settings.dirty = dirty;
  state.audio.buzzerEnabled = settings.buzzer;
  if(changed) markChanged(StateChange::Settings);
}

void markSettingsDirty(bool dirty){
  if(state.settings.dirty == dirty) return;
  state.settings.dirty = dirty;
  markChanged(StateChange::Settings);
}

void setTimerDuration(uint32_t durationMs){
  if(durationMs == 0) durationMs = 1000UL;
  if(state.timer.durationMs == durationMs && state.timer.remainingMs == durationMs) return;

  state.timer.durationMs = durationMs;
  if(!state.timer.active){
    state.timer.remainingMs = durationMs;
  }
  markChanged(StateChange::Timer);
}

void setTimerClockFields(uint8_t hours, uint8_t minutes, uint8_t seconds){
  if(hours > 99) hours = 99;
  if(minutes > 59) minutes = 59;
  if(seconds > 59) seconds = 59;

  const uint32_t duration = timerDurationFromFields(hours, minutes, seconds);
  if(state.timer.hours == hours &&
     state.timer.minutes == minutes &&
     state.timer.seconds == seconds &&
     state.timer.durationMs == duration){
    return;
  }

  state.timer.hours = hours;
  state.timer.minutes = minutes;
  state.timer.seconds = seconds;
  state.timer.durationMs = duration;
  if(!state.timer.active){
    state.timer.remainingMs = duration;
  }
  markChanged(StateChange::Timer);
}

void setTimerEditField(TimerEditField field){
  if(state.timer.editField == field) return;
  state.timer.editField = field;
  markChanged(StateChange::Timer);
}

void startTimer(uint32_t nowMs){
  if(state.timer.remainingMs == 0) state.timer.remainingMs = state.timer.durationMs;
  state.timer.active = true;
  state.timer.alarmActive = false;
  state.timer.startedAtMs = nowMs;
  state.timer.endsAtMs = nowMs + state.timer.remainingMs;
  markChanged(StateChange::Timer);
}

void pauseTimer(uint32_t nowMs){
  if(state.timer.active){
    state.timer.remainingMs = nowMs >= state.timer.endsAtMs ? 0 : state.timer.endsAtMs - nowMs;
  }
  if(!state.timer.active && !state.timer.alarmActive) return;
  state.timer.active = false;
  state.timer.alarmActive = false;
  markChanged(StateChange::Timer);
}

void resetTimer(){
  state.timer.active = false;
  state.timer.alarmActive = false;
  state.timer.remainingMs = state.timer.durationMs;
  markChanged(StateChange::Timer);
}

void completeTimer(uint32_t nowMs){
  state.timer.active = false;
  state.timer.remainingMs = 0;
  state.timer.endsAtMs = nowMs;
  markChanged(StateChange::Timer);
}

void startTimerAlarm(uint32_t nowMs){
  state.timer.alarmActive = true;
  state.timer.alarmStartedAtMs = nowMs;
  state.timer.lastAlarmBeepMs = 0;
  markChanged(StateChange::Timer);
}

void stopTimerAlarm(bool restoreDuration){
  state.timer.alarmActive = false;
  if(restoreDuration){
    state.timer.remainingMs = state.timer.durationMs;
  }
  markChanged(StateChange::Timer);
}

void updateTimerRemaining(uint32_t nowMs){
  if(!state.timer.active) return;
  const uint32_t remaining = nowMs >= state.timer.endsAtMs ? 0 : state.timer.endsAtMs - nowMs;
  if(state.timer.remainingMs == remaining) return;
  state.timer.remainingMs = remaining;
  markChanged(StateChange::Timer);
}

void markTimerAlarmBeep(uint32_t nowMs){
  state.timer.lastAlarmBeepMs = nowMs;
}

void setWifiConnected(bool connected, int32_t rssi){
  if(state.connectivity.wifiConnected == connected && state.connectivity.rssi == rssi) return;
  state.connectivity.wifiConnected = connected;
  state.connectivity.rssi = rssi;
  markChanged(StateChange::Connectivity);
}

void setEsp8266Connected(bool connected){
  if(state.connectivity.esp8266Connected == connected &&
     state.protocol.esp8266Connected == connected){
    return;
  }
  state.connectivity.esp8266Connected = connected;
  state.protocol.esp8266Connected = connected;
  markChanged(StateChange::Connectivity);
}

void setAudioVolume(uint8_t volume){
  if(state.audio.volume == volume) return;
  state.audio.volume = volume;
  state.audio.muted = volume == 0;
  markChanged(StateChange::Audio);
}

void setAudioMuted(bool muted){
  if(state.audio.muted == muted) return;
  state.audio.muted = muted;
  markChanged(StateChange::Audio);
}

void setBuzzerEnabled(bool enabled){
  if(state.audio.buzzerEnabled == enabled) return;
  state.audio.buzzerEnabled = enabled;
  markChanged(StateChange::Audio);
}

void markProtocolSynced(uint32_t revision, uint16_t sequenceId){
  state.protocol.lastSyncRevision = revision;
  state.protocol.lastSequenceId = sequenceId;
  markChanged(StateChange::Protocol);
  state.protocol.lastSyncRevision = stateRevision;
}

uint32_t revision(){
  return stateRevision;
}

uint16_t lastChangeMask(){
  return lastMask;
}

}  // namespace SystemStateStore
