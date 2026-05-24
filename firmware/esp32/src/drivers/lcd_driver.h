#pragma once

#include <Arduino.h>

namespace LcdDriver {
void begin();
void createBlockChar();
void clear();
void writeRow(uint8_t row, const char* text);
void setBacklight(bool enabled);
}

