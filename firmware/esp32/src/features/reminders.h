#pragma once

#include <Arduino.h>

enum ReminderEditField { REM_EDIT_HOUR, REM_EDIT_MINUTE };

namespace RemindersFeature {
constexpr uint8_t MAX_REMINDERS = 5;

void load();
void save();
void check(bool alarmActive);

uint8_t selectedIndex();
void rotateSelected(int step);

ReminderEditField editField();
void advanceEditField();
void adjustSelected(int step);
void toggleSelectedActive();
bool setReminder(uint8_t index, uint8_t hour, uint8_t minute, bool active);

bool selectedActive();
uint8_t selectedHour();
uint8_t selectedMinute();

bool getNext(uint8_t &hour, uint8_t &minute);

void startAlarm(uint8_t idx, unsigned long now);
void stopAlarm();
bool updateAlarm(unsigned long now);
uint8_t activeAlarmIndex();
uint8_t activeAlarmHour();
uint8_t activeAlarmMinute();
}
