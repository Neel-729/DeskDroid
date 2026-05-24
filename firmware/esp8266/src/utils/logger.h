#pragma once

#include <Arduino.h>

class Logger {
 public:
  explicit Logger(Stream& stream);

  void setEnabled(bool enabled);
  void info(const __FlashStringHelper* message);

 private:
  Stream& stream_;
  bool enabled_ = false;
};

