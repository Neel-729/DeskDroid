#pragma once

#include <Arduino.h>

namespace Esp8266Link {

void begin(Stream& stream);
void update();
bool isLinked();

}  // namespace Esp8266Link

