#pragma once

#include <Arduino.h>

#include "../core/settings_store.h"

namespace SettingsFlow {

enum DateField {
  DATE_DAY,
  DATE_MONTH,
  DATE_YEAR
};

struct Snapshot {
  const char* selectedLabel;
  const char* firmwareVersion;
  uint8_t selectedIndex;
  bool blinkState;
  bool scheduleHourSelected;
  bool adjustHourSelected;
  uint8_t adjustHour;
  uint8_t adjustMinute;
  uint16_t adjustYear;
  uint8_t adjustMonth;
  uint8_t adjustDay;
  uint8_t adjustDateField;
};

void begin();
DeviceSettings &settings();
void save();

void rotateMenu(int step);
void beginEdit();
void adjustValue(int step);
void advanceField();
void commitEdit(unsigned long now);
void leaveMenu();
void exitToClock();

bool shouldBlink();
Snapshot snapshot(const char* firmwareVersion, bool blinkState);

}
