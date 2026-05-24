#include "logger.h"

Logger::Logger(Stream& stream) : stream_(stream) {}

void Logger::setEnabled(bool enabled) {
  enabled_ = enabled;
}

void Logger::info(const __FlashStringHelper* message) {
  if (!enabled_) {
    return;
  }

  stream_.print(F("<LOG|INFO|"));
  stream_.print(message);
  stream_.println(F(">"));
}

