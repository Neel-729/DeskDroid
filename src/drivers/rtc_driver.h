#pragma once

#include <RTClib.h>

namespace RtcDriver {
bool begin();
bool isRunning();
DateTime now();
void adjust(const DateTime &value);
}

