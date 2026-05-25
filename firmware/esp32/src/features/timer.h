#pragma once

#include <Arduino.h>

#include "../core/system_state.h"

namespace TimerFeature {
void begin();

void start(unsigned long now);
void pause(unsigned long now);
void reset();
void checkDone(unsigned long now);

bool isRunning();
unsigned long remainingMillis(unsigned long now);
unsigned long totalMillis();

uint8_t hours();
uint8_t minutes();
uint8_t seconds();

void adjustEdit(int step);
void advanceEditField();
TimerEditField editField();

void startAlarm(unsigned long now);
void stopAlarm(bool restoreDuration);
bool shouldAlarmBeep(unsigned long now);
bool alarmTimedOut(unsigned long now);
}
