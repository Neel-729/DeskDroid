#pragma once

#include <Arduino.h>

namespace AudioService {

void service();
void beep(uint16_t durationMs);

}  // namespace AudioService
