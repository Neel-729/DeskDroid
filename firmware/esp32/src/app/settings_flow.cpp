#include "settings_flow.h"

#include "hardware_requests.h"
#include "navigation.h"
#include "../core/time_service.h"
#include "../features/clock.h"
#include "../features/lighting.h"
#include "../utils/date_utils.h"

namespace {

DeviceSettings deviceSettings;

const char* settingsMenu[]={
  "Backlight",
  "LED Mode",
  "LED Brightness",
  "Auto Lights",
  "Lights On",
  "Lights Off",
  "Buzzer",
  "Quotes",
  "Time Format",
  "Adjust Time",
  "Adjust Date",
  "About"
};

const uint8_t SETTINGS_COUNT = sizeof(settingsMenu)/sizeof(settingsMenu[0]);

uint8_t settingsIndex=0;
uint8_t adjustHour=0;
uint8_t adjustMinute=0;
bool adjustHourField=true;

enum ScheduleEditField {
  SCHEDULE_HOUR,
  SCHEDULE_MINUTE
};

ScheduleEditField scheduleEditField = SCHEDULE_HOUR;
uint16_t adjustYear=2025;
uint8_t adjustMonth=1;
uint8_t adjustDay=1;
SettingsFlow::DateField adjustDateField = SettingsFlow::DATE_DAY;

void sanitizeLoadedSettings(){
  if(deviceSettings.lightsOnHour > 23) deviceSettings.lightsOnHour = 7;
  if(deviceSettings.lightsOnMinute > 59) deviceSettings.lightsOnMinute = 0;
  if(deviceSettings.lightsOffHour > 23) deviceSettings.lightsOffHour = 22;
  if(deviceSettings.lightsOffMinute > 59) deviceSettings.lightsOffMinute = 0;
  if(deviceSettings.ledBrightness > 10) deviceSettings.ledBrightness = 6;
  if(deviceSettings.idlePreset > IDLE_PULSE) deviceSettings.idlePreset = IDLE_STATIC;
}

void resetEditModes(){
  adjustHourField=true;
  adjustDateField=SettingsFlow::DATE_DAY;
  scheduleEditField=SCHEDULE_HOUR;
}

}

namespace SettingsFlow {

void begin(){
  SettingsStore::loadDeviceSettings(deviceSettings);
  sanitizeLoadedSettings();
}

DeviceSettings &settings(){
  return deviceSettings;
}

void save(){
  SettingsStore::saveDeviceSettings(deviceSettings);
}

void rotateMenu(int step){
  int newIndex = (int)settingsIndex + step;
  if(newIndex < 0) newIndex = SETTINGS_COUNT-1;
  if(newIndex >= SETTINGS_COUNT) newIndex = 0;
  settingsIndex = (uint8_t)newIndex;
  AppNavigation::markChanged();
}

void beginEdit(){
  resetEditModes();

  if(settingsIndex==9){
    DateTime n=TimeService::now();
    adjustHour=n.hour();
    adjustMinute=n.minute();
  }
  else if(settingsIndex==10){
    DateTime n=TimeService::now();
    adjustYear=n.year();
    adjustMonth=n.month();
    adjustDay=n.day();
  }

  AppNavigation::enter(STATE_SETTINGS_EDIT);
}

void adjustValue(int step){
  if(settingsIndex==4){
    if(scheduleEditField==SCHEDULE_HOUR){
      int h = (int)deviceSettings.lightsOnHour + step;
      if(h < 0) h = 23;
      if(h > 23) h = 0;
      deviceSettings.lightsOnHour = (uint8_t)h;
    } else {
      int m = (int)deviceSettings.lightsOnMinute + step;
      if(m < 0) m = 59;
      if(m > 59) m = 0;
      deviceSettings.lightsOnMinute = (uint8_t)m;
    }
    LightingFeature::refresh(deviceSettings);
  }
  else if(settingsIndex==5){
    if(scheduleEditField==SCHEDULE_HOUR){
      int h = (int)deviceSettings.lightsOffHour + step;
      if(h < 0) h = 23;
      if(h > 23) h = 0;
      deviceSettings.lightsOffHour = (uint8_t)h;
    } else {
      int m = (int)deviceSettings.lightsOffMinute + step;
      if(m < 0) m = 59;
      if(m > 59) m = 0;
      deviceSettings.lightsOffMinute = (uint8_t)m;
    }
    LightingFeature::refresh(deviceSettings);
  }
  else if(settingsIndex==9){
    if(adjustHourField) adjustHour=(adjustHour+step+24)%24;
    else adjustMinute=(adjustMinute+step+60)%60;
  }
  else if(settingsIndex==10){
    if(adjustDateField==DATE_DAY){
      int maxd = DateUtils::daysInMonth(adjustYear, adjustMonth);
      int d = (int)adjustDay + step;
      if(d < 1) d = maxd;
      if(d > maxd) d = 1;
      adjustDay = (uint8_t)d;
    }
    else if(adjustDateField==DATE_MONTH){
      int mo = (int)adjustMonth + step;
      if(mo < 1) mo = 12;
      if(mo > 12) mo = 1;
      adjustMonth = (uint8_t)mo;
      int maxd = DateUtils::daysInMonth(adjustYear, adjustMonth);
      if(adjustDay > maxd) adjustDay = maxd;
    }
    else{
      int y = (int)adjustYear + step;
      if(y < 2000) y = 2000;
      if(y > 2099) y = 2099;
      adjustYear = (uint16_t)y;
      int maxd = DateUtils::daysInMonth(adjustYear, adjustMonth);
      if(adjustDay > maxd) adjustDay = maxd;
    }
  }
  else if(settingsIndex==0){
    deviceSettings.brightness = !deviceSettings.brightness;
    LightingFeature::refresh(deviceSettings);
  }
  else if(settingsIndex==1){
    int val = deviceSettings.idlePreset + step;
    if(val < 0) val = IDLE_PULSE;
    if(val > IDLE_PULSE) val = IDLE_OFF;
    deviceSettings.idlePreset = val;
    HardwareRequests::requestIdlePreset((LedIdlePreset)val, CommandSource::SETTINGS);
  }
  else if(settingsIndex==2){
    int val = deviceSettings.ledBrightness + step;
    if(val < 0) val = 0;
    if(val > 10) val = 10;
    deviceSettings.ledBrightness = val;
    HardwareRequests::requestLedBrightness(deviceSettings.ledBrightness, CommandSource::SETTINGS);
  }
  else if(settingsIndex==3){
    deviceSettings.autoLights=!deviceSettings.autoLights;
    LightingFeature::refresh(deviceSettings);
  }
  else if(settingsIndex==6) deviceSettings.buzzer=!deviceSettings.buzzer;
  else if(settingsIndex==7) deviceSettings.quotes=!deviceSettings.quotes;
  else if(settingsIndex==8) deviceSettings.format24=!deviceSettings.format24;

  AppNavigation::markChanged();
}

void advanceField(){
  if(settingsIndex==4 || settingsIndex==5){
    scheduleEditField = (scheduleEditField==SCHEDULE_HOUR) ? SCHEDULE_MINUTE : SCHEDULE_HOUR;
  }
  else if(settingsIndex==9){
    adjustHourField=!adjustHourField;
  }
  else if(settingsIndex==10){
    if(adjustDateField==DATE_DAY) adjustDateField=DATE_MONTH;
    else if(adjustDateField==DATE_MONTH) adjustDateField=DATE_YEAR;
    else adjustDateField=DATE_DAY;
  }
  else{
    adjustValue(1);
  }
  AppNavigation::markChanged();
}

void commitEdit(unsigned long now){
  if(settingsIndex==9){
    DateTime current = TimeService::now();
    TimeService::adjust(DateTime(current.year(),current.month(),current.day(),adjustHour,adjustMinute,0));
    ClockFeature::syncToRtc(now);
  }
  else if(settingsIndex==10){
    DateTime current = TimeService::now();
    TimeService::adjust(DateTime(adjustYear,adjustMonth,adjustDay,current.hour(),current.minute(),0));
    ClockFeature::syncToRtc(now);
  }

  save();
  LightingFeature::refresh(deviceSettings);
  resetEditModes();
  AppNavigation::enter(STATE_SETTINGS_MENU);
}

void leaveMenu(){
  resetEditModes();
  AppNavigation::enter(STATE_SETTINGS_HOME);
}

void exitToClock(){
  resetEditModes();
  AppNavigation::enter(STATE_CLOCK);
}

bool shouldBlink(){
  return settingsIndex==4 || settingsIndex==5 || settingsIndex==9 || settingsIndex==10;
}

Snapshot snapshot(const char* firmwareVersion, bool blinkState){
  Snapshot current = {
    settingsMenu[settingsIndex],
    firmwareVersion,
    settingsIndex,
    blinkState,
    scheduleEditField==SCHEDULE_HOUR,
    adjustHourField,
    adjustHour,
    adjustMinute,
    adjustYear,
    adjustMonth,
    adjustDay,
    (uint8_t)adjustDateField
  };
  return current;
}

}
