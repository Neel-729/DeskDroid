#include "system_state.h"

namespace {

SystemState state;
uint32_t stateRevision = 0;
LedIdlePreset currentIdlePreset = IDLE_STATIC;
LedState currentLedMode = LED_IDLE;

uint8_t brightnessLevelToByte(uint8_t level){
  if(level > 10) level = 10;
  return static_cast<uint8_t>(level * 25);
}

void markChanged(){
  stateRevision++;
  if(stateRevision == 0) stateRevision = 1;
}

void applyIdlePreset(LedIdlePreset preset){
  switch(preset){
    case IDLE_OFF:
      state.ledsEnabled = false;
      state.currentEffect = EffectType::None;
      state.currentColor = RGBColor(0, 0, 0);
      break;

    case IDLE_STATIC:
      state.ledsEnabled = true;
      state.currentEffect = EffectType::Solid;
      state.currentColor = RGBColor(30, 0, 20);
      break;

    case IDLE_BREATH:
    case IDLE_PULSE:
      state.ledsEnabled = true;
      state.currentEffect = EffectType::Breathing;
      state.currentColor = RGBColor(30, 0, 20);
      break;

    case IDLE_RAINBOW:
      state.ledsEnabled = true;
      state.currentEffect = EffectType::Rainbow;
      state.currentColor = RGBColor(30, 0, 20);
      break;
  }
}

}  // namespace

namespace SystemStateStore {

void begin(const DeviceSettings &settings){
  state = {};
  state.ledsEnabled = true;
  state.brightness = brightnessLevelToByte(settings.ledBrightness);
  currentIdlePreset = static_cast<LedIdlePreset>(settings.idlePreset);
  currentLedMode = LED_IDLE;
  applyIdlePreset(currentIdlePreset);
  markChanged();
}

const SystemState &current(){
  return state;
}

bool setRelay(uint8_t relayNumber, bool enabled){
  if(relayNumber == 0 || relayNumber > SystemState::RelayCount) return false;

  bool &target = state.relayStates[relayNumber - 1];
  if(target == enabled) return true;

  target = enabled;
  markChanged();
  return true;
}

void setLedsEnabled(bool enabled){
  if(state.ledsEnabled == enabled) return;
  state.ledsEnabled = enabled;
  markChanged();
}

void setBrightnessLevel(uint8_t level){
  const uint8_t brightness = brightnessLevelToByte(level);
  if(state.brightness == brightness) return;

  state.brightness = brightness;
  markChanged();
}

void setIdlePreset(LedIdlePreset preset){
  currentIdlePreset = preset;
  if(currentLedMode != LED_IDLE) return;

  const SystemState before = state;
  applyIdlePreset(preset);
  if(before.ledsEnabled != state.ledsEnabled ||
     before.currentEffect != state.currentEffect ||
     before.currentColor.r != state.currentColor.r ||
     before.currentColor.g != state.currentColor.g ||
     before.currentColor.b != state.currentColor.b){
    markChanged();
  }
}

void setLedMode(LedState mode){
  currentLedMode = mode;
  const SystemState before = state;

  switch(mode){
    case LED_IDLE:
      applyIdlePreset(currentIdlePreset);
      break;

    case LED_TIMER_ALARM:
      state.ledsEnabled = true;
      state.currentEffect = EffectType::Solid;
      state.currentColor = RGBColor(255, 0, 0);
      break;

    case LED_REMINDER_ALARM:
      state.ledsEnabled = true;
      state.currentEffect = EffectType::Solid;
      state.currentColor = RGBColor(255, 80, 0);
      break;

    case LED_SUCCESS:
      state.ledsEnabled = true;
      state.currentEffect = EffectType::Solid;
      state.currentColor = RGBColor(0, 255, 0);
      break;
  }

  if(before.ledsEnabled != state.ledsEnabled ||
     before.currentEffect != state.currentEffect ||
     before.currentColor.r != state.currentColor.r ||
     before.currentColor.g != state.currentColor.g ||
     before.currentColor.b != state.currentColor.b){
    markChanged();
  }
}

uint32_t revision(){
  return stateRevision;
}

}  // namespace SystemStateStore
