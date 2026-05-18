#pragma once

#include <RTClib.h>

namespace TimeService {
bool begin();
bool isRunning();
DateTime now();
void adjust(const DateTime &value);
}
