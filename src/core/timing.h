#pragma once

#include <Arduino.h>

namespace Timing {
bool intervalElapsed(unsigned long now, unsigned long &lastTick, unsigned long intervalMs);
}
