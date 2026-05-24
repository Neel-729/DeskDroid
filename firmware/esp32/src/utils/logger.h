#pragma once

#include <Arduino.h>

namespace Logger {

void info(Stream& stream, const __FlashStringHelper* message);

}  // namespace Logger

