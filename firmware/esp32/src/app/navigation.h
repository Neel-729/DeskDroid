#pragma once

#include <Arduino.h>

#include "app_state.h"

namespace AppNavigation {
AppState current();
AppState resumeAfterReminder();

void commit();
void enter(AppState nextState);
void setResumeAfterReminder(AppState state);
void rotateMainState(int step);

bool hasStateChanged();
void clearStateChanged();
void markChanged();

// New stack-based navigation
void back();
void goHome();
bool isAtHome();
uint8_t stackDepth();
}  // namespace AppNavigation