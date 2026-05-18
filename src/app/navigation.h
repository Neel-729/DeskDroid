#pragma once

#include <Arduino.h>

#include "app_state.h"

namespace AppNavigation {
AppState current();
AppState resumeAfterReminder();

void enter(AppState nextState);
void setResumeAfterReminder(AppState state);
void rotateMainState(int step);

bool hasStateChanged();
void clearStateChanged();
void markChanged();
}
