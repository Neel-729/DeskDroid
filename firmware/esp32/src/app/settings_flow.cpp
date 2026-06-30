#include "settings_flow.h"

#include "application_commands.h"
#include "idle_manager.h"
#include "navigation.h"
#include "../core/system_state.h"
#include "../core/time_service.h"
#include "../features/clock.h"
#include "../services/lighting_service.h"
#include "../utils/date_utils.h"

namespace {

DeviceSettings deviceSettings;

const char* settingsMenu[] = {
  "Backlight",
  "LED Mode",
  "LED Brightness",
  "Relays",
  "Auto Lights",
  "Lights On",
  "Lights Off",
  "Auto Return Home",
  "Buzzer",
  "Quotes",
  "Time Format",
  "Adjust Time",
  "Adjust Date",
  "About"
};
const uint8_t SETTINGS_COUNT = sizeof(settingsMenu) / sizeof(settingsMenu[0]);

enum ScheduleEditField {
  SCHEDULE_HOUR,
  SCHEDULE_MINUTE
};
const uint16_t IDLE_TIMEOUT_OPTIONS[] = {0, 15, 30, 60, 120, 300};
const uint8_t IDLE_TIMEOUT_COUNT = sizeof(IDLE_TIMEOUT_OPTIONS) / sizeof(IDLE_TIMEOUT_OPTIONS[0]);

uint8_t settingsIndex = 0;
uint8_t relayIndex = 0;
uint8_t adjustHour = 0;
uint8_t adjustMinute = 0;
bool adjustHourField = true;
ScheduleEditField scheduleEditField = SCHEDULE_HOUR;
uint16_t adjustYear = 2025;
uint8_t adjustMonth = 1;
uint8_t adjustDay = 1;
SettingsFlow::DateField adjustDateField = SettingsFlow::DATE_DAY;
uint8_t adjustIdleTimeoutIndex = 0;

void sanitizeLoadedSettings() {
  if (deviceSettings.lightsOnHour > 23) deviceSettings.lightsOnHour = 7;
  if (deviceSettings.lightsOnMinute > 59) deviceSettings.lightsOnMinute = 0;
  if (deviceSettings.lightsOffHour > 23) deviceSettings.lightsOffHour = 22;
  if (deviceSettings.lightsOffMinute > 59) deviceSettings.lightsOffMinute = 0;
  if (deviceSettings.ledBrightness > 10) deviceSettings.ledBrightness = 6;
  if (deviceSettings.idlePreset > IDLE_AMBIENT) deviceSettings.idlePreset = IDLE_STATIC;
}

void resetEditModes() {
  adjustHourField = true;
  adjustDateField = SettingsFlow::DATE_DAY;
  scheduleEditField = SCHEDULE_HOUR;
  adjustIdleTimeoutIndex = 0;
}

void adjustScheduleTime(int step, uint8_t &hour, uint8_t &minute) {
  if (scheduleEditField == SCHEDULE_HOUR) {
    int h = static_cast<int>(hour) + step;
    if (h < 0) h = 23;
    if (h > 23) h = 0;
    hour = static_cast<uint8_t>(h);
  } else {
    int m = static_cast<int>(minute) + step;
    if (m < 0) m = 59;
    if (m > 59) m = 0;
    minute = static_cast<uint8_t>(m);
  }
  AppCommands::applySettings(deviceSettings);
  LightingService::refreshSchedule(deviceSettings);
}

void adjustTimeValue(int step) {
  if (adjustHourField) {
    adjustHour = (adjustHour + step + 24) % 24;
  } else {
    adjustMinute = (adjustMinute + step + 60) % 60;
  }
}

void adjustDateValue(int step) {
  if (adjustDateField == SettingsFlow::DATE_DAY) {
    int maxd = DateUtils::daysInMonth(adjustYear, adjustMonth);
    int d = static_cast<int>(adjustDay) + step;
    if (d < 1) d = maxd;
    if (d > maxd) d = 1;
    adjustDay = static_cast<uint8_t>(d);
  } else if (adjustDateField == SettingsFlow::DATE_MONTH) {
    int mo = static_cast<int>(adjustMonth) + step;
    if (mo < 1) mo = 12;
    if (mo > 12) mo = 1;
    adjustMonth = static_cast<uint8_t>(mo);
    int maxd = DateUtils::daysInMonth(adjustYear, adjustMonth);
    if (adjustDay > maxd) adjustDay = maxd;
  } else {
    int y = static_cast<int>(adjustYear) + step;
    if (y < 2000) y = 2000;
    if (y > 2099) y = 2099;
    adjustYear = static_cast<uint16_t>(y);
    int maxd = DateUtils::daysInMonth(adjustYear, adjustMonth);
    if (adjustDay > maxd) adjustDay = maxd;
  }
}

template<typename T>
void adjustToggleSetting(T &setting) {
  setting = !setting;
  AppCommands::applySettings(deviceSettings);
}

void adjustLedMode(int step) {
  int val = deviceSettings.idlePreset + step;
  if (val < 0) val = IDLE_AMBIENT;
  if (val > IDLE_AMBIENT) val = IDLE_OFF;
  deviceSettings.idlePreset = val;
  AppCommands::applySettings(deviceSettings);
  AppCommands::setIdlePreset(static_cast<LedIdlePreset>(val));
}

void adjustLedBrightness(int step) {
  int val = deviceSettings.ledBrightness + step;
  if (val < 0) val = 0;
  if (val > 10) val = 10;
  deviceSettings.ledBrightness = static_cast<uint8_t>(val);
  AppCommands::applySettings(deviceSettings);
  AppCommands::setBrightnessLevel(deviceSettings.ledBrightness);
}

void adjustRelayIndex(int step) {
  int nextRelay = static_cast<int>(relayIndex) + step;
  if (nextRelay < 0) nextRelay = SystemState::RelayCount - 1;
  if (nextRelay >= SystemState::RelayCount) nextRelay = 0;
  relayIndex = static_cast<uint8_t>(nextRelay);
}

void adjustIdleTimeout(int step) {
  int newIndex = static_cast<int>(adjustIdleTimeoutIndex) + step;
  if (newIndex < 0) newIndex = IDLE_TIMEOUT_COUNT - 1;
  if (newIndex >= IDLE_TIMEOUT_COUNT) newIndex = 0;
  adjustIdleTimeoutIndex = static_cast<uint8_t>(newIndex);
}

}

namespace SettingsFlow {

void begin() {
  SettingsStore::loadDeviceSettings(deviceSettings);
  sanitizeLoadedSettings();
}

DeviceSettings &settings() {
  return deviceSettings;
}

void save() {
  AppCommands::saveSettings(deviceSettings);
}

void rotateMenu(int step) {
  int newIndex = static_cast<int>(settingsIndex) + step;
  if (newIndex < 0) newIndex = SETTINGS_COUNT - 1;
  if (newIndex >= SETTINGS_COUNT) newIndex = 0;
  settingsIndex = static_cast<uint8_t>(newIndex);
  AppNavigation::markChanged();
}

void beginEdit() {
  resetEditModes();

  if (settingsIndex == 7) {
    for (uint8_t i = 0; i < IDLE_TIMEOUT_COUNT; i++) {
      if (IDLE_TIMEOUT_OPTIONS[i] == deviceSettings.idleTimeoutSeconds) {
        adjustIdleTimeoutIndex = i;
        break;
      }
    }
  } else if (settingsIndex == 11) {
    DateTime n = TimeService::now();
    adjustHour = n.hour();
    adjustMinute = n.minute();
  } else if (settingsIndex == 12) {
    DateTime n = TimeService::now();
    adjustYear = n.year();
    adjustMonth = n.month();
    adjustDay = n.day();
  }

  AppNavigation::enter(STATE_SETTINGS_EDIT);
}

void adjustValue(int step) {
  switch (settingsIndex) {
    case 0:
      adjustToggleSetting(deviceSettings.brightness);
      LightingService::refreshSchedule(deviceSettings);
      break;
    case 1:
      adjustLedMode(step);
      break;
    case 2:
      adjustLedBrightness(step);
      break;
    case 3:
      adjustRelayIndex(step);
      break;
    case 4:
      adjustToggleSetting(deviceSettings.autoLights);
      LightingService::refreshSchedule(deviceSettings);
      break;
    case 5:
      adjustScheduleTime(step, deviceSettings.lightsOnHour, deviceSettings.lightsOnMinute);
      break;
    case 6:
      adjustScheduleTime(step, deviceSettings.lightsOffHour, deviceSettings.lightsOffMinute);
      break;
    case 7:
      adjustIdleTimeout(step);
      break;
    case 8:
      adjustToggleSetting(deviceSettings.buzzer);
      break;
    case 9:
      adjustToggleSetting(deviceSettings.quotes);
      break;
    case 10:
      adjustToggleSetting(deviceSettings.format24);
      break;
    case 11:
      adjustTimeValue(step);
      break;
    case 12:
      adjustDateValue(step);
      break;
  }
  AppNavigation::markChanged();
}

void advanceField() {
  if (settingsIndex == 5 || settingsIndex == 6) {
    scheduleEditField = (scheduleEditField == SCHEDULE_HOUR) ? SCHEDULE_MINUTE : SCHEDULE_HOUR;
  } else if (settingsIndex == 3) {
    const bool enabled = SystemStateStore::current().relayStates[relayIndex];
    AppCommands::setRelay(relayIndex + 1, !enabled);
  } else if (settingsIndex == 7) {
    deviceSettings.idleTimeoutSeconds = IDLE_TIMEOUT_OPTIONS[adjustIdleTimeoutIndex];
    AppCommands::applySettings(deviceSettings);
    IdleManager::setIdleTimeout(static_cast<IdleManager::IdleTimeout>(deviceSettings.idleTimeoutSeconds));
  } else if (settingsIndex == 11) {
    adjustHourField = !adjustHourField;
  } else if (settingsIndex == 12) {
    if (adjustDateField == DATE_DAY) adjustDateField = DATE_MONTH;
    else if (adjustDateField == DATE_MONTH) adjustDateField = DATE_YEAR;
    else adjustDateField = DATE_DAY;
  } else {
    adjustValue(1);
  }
  AppNavigation::markChanged();
}

void commitEdit(unsigned long now) {
  if (settingsIndex == 11) {
    DateTime current = TimeService::now();
    TimeService::adjust(DateTime(current.year(), current.month(), current.day(), adjustHour, adjustMinute, 0));
    ClockFeature::syncToRtc(now);
  } else if (settingsIndex == 12) {
    DateTime current = TimeService::now();
    TimeService::adjust(DateTime(adjustYear, adjustMonth, adjustDay, current.hour(), current.minute(), 0));
    ClockFeature::syncToRtc(now);
  }

  save();
  LightingService::refreshSchedule(deviceSettings);
  resetEditModes();
  AppNavigation::enter(STATE_SETTINGS_MENU);
}

void leaveMenu() {
  resetEditModes();
  AppNavigation::enter(STATE_SETTINGS_HOME);
}

void exitToClock() {
  resetEditModes();
  AppNavigation::enter(STATE_CLOCK);
}

bool shouldBlink() {
  return settingsIndex == 5 || settingsIndex == 6 || settingsIndex == 7 || settingsIndex == 11 || settingsIndex == 12;
}

Snapshot snapshot(const char* firmwareVersion, bool blinkState) {
  Snapshot current = {
    settingsMenu[settingsIndex],
    firmwareVersion,
    settingsIndex,
    blinkState,
    scheduleEditField == SCHEDULE_HOUR,
    adjustHourField,
    adjustHour,
    adjustMinute,
    adjustYear,
    adjustMonth,
    adjustDay,
    static_cast<uint8_t>(adjustDateField),
    relayIndex,
    SystemStateStore::current().relayStates[relayIndex],
    deviceSettings.idleTimeoutSeconds,
    adjustIdleTimeoutIndex
  };
  return current;
}

}