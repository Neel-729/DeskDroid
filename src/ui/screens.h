#pragma once

#include <Arduino.h>

#include "../app/app_state.h"
#include "../app/settings_flow.h"
#include "../core/settings_store.h"

namespace UiScreens {

struct TimerScreenData {
  bool running;
  unsigned long totalMillis;
  unsigned long remainingMillis;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t editField;
};

struct StopwatchScreenData {
  unsigned long elapsedMillis;
};

struct RemindersScreenData {
  uint8_t maxReminders;
  uint8_t selectedIndex;
  bool selectedActive;
  uint8_t selectedHour;
  uint8_t selectedMinute;
  uint8_t editField;
  bool hasNext;
  uint8_t nextHour;
  uint8_t nextMinute;
  uint8_t activeAlarmIndex;
  uint8_t activeAlarmHour;
  uint8_t activeAlarmMinute;
};

const char* row(uint8_t index);
void clearFrame();

void renderBootTitle(const char* firmwareVersion);
void renderBootScreen(const char* firmwareVersion, const char* status);
void renderRtcErrorScreen();
void renderClockScreen(const char* timeRow, const char* quoteRow);
void renderSettingsScreen(AppState state, const DeviceSettings &settings, const SettingsFlow::Snapshot &renderState);
void renderTimerScreen(AppState state, const TimerScreenData &data, bool blinkState);
void renderStopwatchScreen(const StopwatchScreenData &data);
void renderRemindersScreen(AppState state, const RemindersScreenData &data, bool format24, bool blinkState);
void renderReminderAlarmScreen(const RemindersScreenData &data);

}
