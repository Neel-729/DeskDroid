#pragma once

#include <Arduino.h>

namespace StopwatchFeature {
void start(unsigned long now);
void stop(unsigned long now);
void resume(unsigned long now);
void reset();
void update(unsigned long now);
bool isRunning();
unsigned long elapsed();
}