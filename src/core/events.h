#pragma once

#include <Arduino.h>

enum EventType {
  EVENT_NONE,
  EVENT_CLICK,
  EVENT_DOUBLE_CLICK,
  EVENT_LONG_PRESS,
  EVENT_ROTATE_CW,
  EVENT_ROTATE_CCW,
  EVENT_TIMER_DONE,
  EVENT_TIMER_ALARM_TIMEOUT,
  EVENT_REMINDER_TRIGGER,
  EVENT_REMINDER_TIMEOUT
};

struct AppEvent {
  EventType type;
  uint8_t data;
};

bool enqueueEvent(EventType type, uint8_t data=0);
bool dequeueEvent(AppEvent &event);
uint16_t droppedEventCount();

