#pragma once

#include <Arduino.h>

#ifndef DESKDROID_LOG_ENABLED
#define DESKDROID_LOG_ENABLED 1
#endif

#ifndef DESKDROID_LOG_DEBUG_ENABLED
#define DESKDROID_LOG_DEBUG_ENABLED 0
#endif

enum class LogLevel : uint8_t {
  DEBUG,
  INFO,
  WARN,
  ERROR
};

enum class LogTag : uint8_t {
  APP,
  UI,
  LED,
  TIMER,
  REMINDER,
  EVENTS,
  SETTINGS,
  INPUTS,
  RTC,
  SCHED
};

struct RateLimitedLog {
  unsigned long lastMs = 0;
};

namespace Log {
void write(LogTag tag, LogLevel level, const char *format, ...);
bool shouldLog(RateLimitedLog &state, unsigned long intervalMs, unsigned long now);
}

#if DESKDROID_LOG_ENABLED
#define LOG_INFO(tag, fmt, ...) Log::write((tag), LogLevel::INFO, (fmt), ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...) Log::write((tag), LogLevel::WARN, (fmt), ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) Log::write((tag), LogLevel::ERROR, (fmt), ##__VA_ARGS__)
#else
#define LOG_INFO(tag, fmt, ...)
#define LOG_WARN(tag, fmt, ...)
#define LOG_ERROR(tag, fmt, ...)
#endif

#if DESKDROID_LOG_ENABLED && DESKDROID_LOG_DEBUG_ENABLED
#define LOG_DEBUG(tag, fmt, ...) Log::write((tag), LogLevel::DEBUG, (fmt), ##__VA_ARGS__)
#else
#define LOG_DEBUG(tag, fmt, ...)
#endif
