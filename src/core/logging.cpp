#include "logging.h"

#include <stdarg.h>
#include <stdio.h>

namespace {

const char *levelName(LogLevel level){
  switch(level){
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO";
    case LogLevel::WARN: return "WARN";
    case LogLevel::ERROR: return "ERROR";
  }
  return "?";
}

const char *tagName(LogTag tag){
  switch(tag){
    case LogTag::APP: return "APP";
    case LogTag::UI: return "UI";
    case LogTag::LED: return "LED";
    case LogTag::TIMER: return "TIMER";
    case LogTag::REMINDER: return "REMINDER";
    case LogTag::EVENTS: return "EVENTS";
    case LogTag::SETTINGS: return "SETTINGS";
    case LogTag::INPUTS: return "INPUT";
    case LogTag::RTC: return "RTC";
    case LogTag::SCHED: return "SCHED";
  }
  return "?";
}

}

namespace Log {

void write(LogTag tag, LogLevel level, const char *format, ...){
#if DESKDROID_LOG_ENABLED
  Serial.printf("[%lu][%s][%s] ", millis(), tagName(tag), levelName(level));

  va_list args;
  va_start(args, format);
  char message[128];
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  Serial.println(message);
#else
  (void)tag;
  (void)level;
  (void)format;
#endif
}

bool shouldLog(RateLimitedLog &state, unsigned long intervalMs, unsigned long now){
  if(now - state.lastMs < intervalMs) return false;
  state.lastMs = now;
  return true;
}

}
