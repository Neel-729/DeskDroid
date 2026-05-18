#pragma once

#include <Arduino.h>

namespace StopwatchFeature {
void toggle(unsigned long now);
void reset();
void update(unsigned long now);
bool isRunning();
unsigned long elapsed();
}

