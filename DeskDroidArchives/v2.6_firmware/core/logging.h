#pragma once

#include <Arduino.h>

enum class LogTag : uint8_t {
  APP,
  EVENTS,
  LED
};

struct RateLimitedLog {
  unsigned long lastLogMs = 0;
};

namespace Log {
bool shouldLog(RateLimitedLog &state, unsigned long intervalMs, unsigned long now);
const char* tagName(LogTag tag);
}

#define LOG_INFO(tag, format, ...) \
  do { \
    Serial.printf("[%s] " format "\n", Log::tagName(tag), ##__VA_ARGS__); \
  } while(0)

#define LOG_WARN(tag, format, ...) \
  do { \
    Serial.printf("[%s][WARN] " format "\n", Log::tagName(tag), ##__VA_ARGS__); \
  } while(0)
