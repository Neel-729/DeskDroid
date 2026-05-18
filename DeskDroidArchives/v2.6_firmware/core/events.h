#pragma once

#include <Arduino.h>

enum EventType : uint8_t {
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

enum class EventSource : uint8_t {
  INPUTS,
  TIMER,
  REMINDER,
  SYSTEM,
  UI
};

struct EncoderEventPayload {
  int8_t direction;
};

struct TimerEventPayload {
  uint8_t reserved;
};

struct ReminderEventPayload {
  uint8_t index;
};

struct AppEvent {
  EventType type;
  EventSource source;
  uint32_t timestamp;
  uint16_t sequenceId;

  union {
    EncoderEventPayload encoder;
    TimerEventPayload timer;
    ReminderEventPayload reminder;
  } payload;
};

struct EventQueueStats {
  uint32_t queued;
  uint32_t dequeued;
  uint16_t dropped;
  uint8_t maxDepth;
  uint16_t lastSequenceId;
};

bool enqueueEvent(const AppEvent &event);
bool enqueueEvent(EventType type, EventSource source = EventSource::SYSTEM);
bool enqueueEncoderEvent(EventType type, int8_t direction);
bool enqueueTimerEvent(EventType type);
bool enqueueReminderEvent(EventType type, uint8_t index);
bool dequeueEvent(AppEvent &event);
uint16_t droppedEventCount();
const EventQueueStats &eventQueueStats();
