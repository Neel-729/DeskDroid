#include "lighting.h"

#include "../core/time_service.h"

namespace {

bool lightScheduleAllowsOutput = true;
LedState requestedLedState = LED_IDLE;

uint16_t minutesSinceMidnight(uint8_t hour, uint8_t minute){
  return (uint16_t)hour * 60 + minute;
}

bool isWithinLightSchedule(const DeviceSettings &settings, const DateTime &now){
  uint16_t current = minutesSinceMidnight(now.hour(), now.minute());
  uint16_t start = minutesSinceMidnight(settings.lightsOnHour, settings.lightsOnMinute);
  uint16_t end = minutesSinceMidnight(settings.lightsOffHour, settings.lightsOffMinute);

  if(start == end) return true;
  if(start < end) return current >= start && current < end;
  return current >= start || current < end;
}

}

namespace LightingFeature {

void begin(const DeviceSettings &settings){
  requestedLedState = LED_IDLE;
  refreshSchedule(settings);
}

void refreshSchedule(const DeviceSettings &settings){
  lightScheduleAllowsOutput = !settings.autoLights || isWithinLightSchedule(settings, TimeService::now());
}

void refresh(const DeviceSettings &settings){
  refreshSchedule(settings);
}

void requestMode(LedState mode){
  requestedLedState = mode;
}

LedState requestedMode(){
  return requestedLedState;
}

bool allowsOutput(){
  return lightScheduleAllowsOutput;
}

bool backlightEnabled(const DeviceSettings &settings){
  return settings.brightness && lightScheduleAllowsOutput;
}

}
