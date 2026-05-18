#pragma once

#include <Arduino.h>

#include "../app/app_state.h"
#include "../app/settings_flow.h"
#include "../core/settings_store.h"

namespace UiScreens {

void renderBootTitle(const char* firmwareVersion);
void renderBootScreen(const char* firmwareVersion, const char* status);
void renderRtcErrorScreen();
void renderClockScreen(const char* timeRow, const char* quoteRow);
void renderSettingsScreen(AppState state, const DeviceSettings &settings, const SettingsFlow::Snapshot &renderState);
void renderTimerScreen(AppState state, unsigned long now, bool blinkState);
void renderStopwatchScreen();
void renderRemindersScreen(AppState state, bool format24, bool blinkState);
void renderReminderAlarmScreen();

}
