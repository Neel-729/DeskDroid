#pragma once

#include <Arduino.h>

#include "../core/system_state.h"

namespace TimerFeature {
void begin();

void enterEditing();
void exitEditing();
void enterRunning(unsigned long now);
void enterPaused(unsigned long now);
void enterComplete(unsigned long now);
void confirmReset();
void update(unsigned long now);

bool isRunning();
bool isPaused();
bool isEditing();
bool isComplete();
bool isIdle();

unsigned long remainingMillis(unsigned long now);
unsigned long totalMillis();

uint8_t hours();
uint8_t minutes();
uint8_t seconds();

void adjustEdit(int step);
void advanceEditField();
TimerEditField editField();

bool shouldAlarmBeep(unsigned long now);
bool alarmTimedOut(unsigned long now);
}