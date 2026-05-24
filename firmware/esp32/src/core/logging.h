#pragma once

#include <Arduino.h>

enum class LogTag : uint8_t {
  APP,
  EVENTS,
  LED,
  LINK,
  SYNC,
  STATE,
  HEARTBEAT,
  PROTO,
  SYSTEM
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

#ifndef DESKDROID_CONTROL_LOGGING
#define DESKDROID_CONTROL_LOGGING 0
#endif

#define CONTROL_LOG_INFO(tag, format, ...) \
  do { \
    if(DESKDROID_CONTROL_LOGGING) { \
      Serial.printf("[%s] " format "\n", Log::tagName(tag), ##__VA_ARGS__); \
    } \
  } while(0)

#define CONTROL_LOG_WARN(tag, format, ...) \
  do { \
    if(DESKDROID_CONTROL_LOGGING) { \
      Serial.printf("[%s][WARN] " format "\n", Log::tagName(tag), ##__VA_ARGS__); \
    } \
  } while(0)
