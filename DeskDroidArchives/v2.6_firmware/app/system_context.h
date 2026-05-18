#pragma once

#include <Arduino.h>

struct UIContext {
  unsigned long lastRefreshMs = 0;
  unsigned long lastBlinkMs = 0;
  bool blinkState = true;
};

struct LightingContext {
  bool driverModeDirty = true;
};

struct TimerContext {
  bool reserved = false;
};

struct ReminderContext {
  bool reserved = false;
};

struct SettingsContext {
  bool reserved = false;
};

struct SystemContext {
  UIContext ui;
  LightingContext lighting;
  TimerContext timer;
  ReminderContext reminders;
  SettingsContext settings;
};
