#pragma once

#include <Arduino.h>

struct DeviceSettings {
  bool buzzer=true;
  bool quotes=true;
  bool format24=true;
  uint8_t brightness=4;
  uint8_t ledBrightness=5;
  uint8_t idlePreset = 1;
  bool autoLights = true;
  uint8_t lightsOnHour = 7;
  uint8_t lightsOnMinute = 0;
  uint8_t lightsOffHour = 22;
  uint8_t lightsOffMinute = 0;
  uint8_t idleTimeoutSeconds = 30;  // Auto-return to dashboard timeout (0=OFF)
};

struct ReminderRecord {
  uint8_t hour;
  uint8_t minute;
  bool active;
};

namespace SettingsStore {
void begin();
void loadDeviceSettings(DeviceSettings &settings);
void saveDeviceSettings(const DeviceSettings &settings);
void loadReminders(ReminderRecord reminders[], uint8_t count);
void saveReminders(const ReminderRecord reminders[], uint8_t count);
}

