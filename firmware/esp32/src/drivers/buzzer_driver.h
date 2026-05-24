#pragma once

#include <Arduino.h>

namespace BuzzerDriver {
void begin();
void trigger(int duration=50);
void update();
}

