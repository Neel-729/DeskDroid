#include "lighting_service.h"

#include "../app/hardware_requests.h"
#include "../core/system_state.h"
#include "../core/time_service.h"

namespace {

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

}  // namespace

namespace LightingService {

void begin(const DeviceSettings &settings){
  refreshSchedule(settings);
}

void refreshSchedule(const DeviceSettings &settings){
  const bool scheduleAllowsOutput = !settings.autoLights || isWithinLightSchedule(settings, TimeService::now());
  SystemStateStore::setLightingScheduleAllowed(scheduleAllowsOutput);
  SystemStateStore::setBacklightEnabled(settings.brightness && scheduleAllowsOutput);
}

void handleEvent(const AppEvent &event){
  if(event.type != EVENT_STATE_CHANGED) return;
  if((event.payload.state.changeMask & static_cast<uint16_t>(StateChange::Lighting)) == 0) return;

  const LightingState &lighting = SystemStateStore::current().lighting;
  HardwareRequests::requestBacklight(lighting.backlightEnabled, CommandSource::LIGHTING);
}

}  // namespace LightingService
