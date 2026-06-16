#include "events.h"

#include "logging.h"

namespace {
constexpr uint8_t EVENT_QUEUE_SIZE = 64;
AppEvent eventQueue[EVENT_QUEUE_SIZE];
uint8_t eventQueueHead = 0;
uint8_t eventQueueTail = 0;
EventQueueStats stats = {0, 0, 0, 0, 0};
RateLimitedLog droppedEventLog;
uint16_t nextSequenceId = 1;

uint8_t queueDepth(){
  if(eventQueueHead >= eventQueueTail) return eventQueueHead - eventQueueTail;
  return EVENT_QUEUE_SIZE - eventQueueTail + eventQueueHead;
}
}

bool enqueueEvent(const AppEvent &event){
  if(event.type==EVENT_NONE) return true;

  uint8_t nextHead = (eventQueueHead + 1) % EVENT_QUEUE_SIZE;
  if(nextHead == eventQueueTail){
    stats.dropped++;
    if(Log::shouldLog(droppedEventLog, 2000, millis())){
      LOG_WARN(LogTag::EVENTS, "Queue full, dropped=%u", stats.dropped);
    }
    return false;
  }

  AppEvent stampedEvent = event;
  stampedEvent.timestamp = millis();
  stampedEvent.sequenceId = nextSequenceId++;
  if(nextSequenceId == 0) nextSequenceId = 1;

  eventQueue[eventQueueHead] = stampedEvent;
  eventQueueHead = nextHead;
  stats.queued++;
  stats.lastSequenceId = stampedEvent.sequenceId;
  uint8_t depth = queueDepth();
  if(depth > stats.maxDepth){
    stats.maxDepth = depth;
  }
  return true;
}

bool enqueueEvent(EventType type, EventSource source){
  AppEvent event = {};
  event.type = type;
  event.source = source;
  return enqueueEvent(event);
}

bool enqueueEncoderEvent(EventType type, int8_t direction){  
  if (eventQueueHead != eventQueueTail) {
    uint8_t prevHead = (eventQueueHead == 0) ? (EVENT_QUEUE_SIZE - 1) : (eventQueueHead - 1);
    if (eventQueue[prevHead].type == type && eventQueue[prevHead].source == EventSource::INPUTS) {
      eventQueue[prevHead].payload.encoder.direction += direction;
      return true;
    }
  }

  AppEvent event = {};
  event.type = type;
  event.source = EventSource::INPUTS;
  event.payload.encoder.direction = direction;
  return enqueueEvent(event);
}

bool enqueueTimerEvent(EventType type){
  AppEvent event = {};
  event.type = type;
  event.source = EventSource::TIMER;
  return enqueueEvent(event);
}

bool enqueueReminderEvent(EventType type, uint8_t index){
  AppEvent event = {};
  event.type = type;
  event.source = EventSource::REMINDER;
  event.payload.reminder.index = index;
  return enqueueEvent(event);
}

bool enqueueStateChangedEvent(uint16_t changeMask, uint32_t revision){
  AppEvent event = {};
  event.type = EVENT_STATE_CHANGED;
  event.source = EventSource::SYSTEM;
  event.payload.state.changeMask = changeMask;
  event.payload.state.revision = revision;
  return enqueueEvent(event);
}

bool dequeueEvent(AppEvent &event){
  if(eventQueueTail == eventQueueHead) return false;

  event = eventQueue[eventQueueTail];
  eventQueueTail = (eventQueueTail + 1) % EVENT_QUEUE_SIZE;
  stats.dequeued++;
  
  // STAGE 4: Log event being popped from queue
  if(event.type == EVENT_CLICK || event.type == EVENT_DOUBLE_CLICK || event.type == EVENT_LONG_PRESS){
    Serial.printf("[QUEUE POP] event=%d\n", event.type);
  }
  return true;
}

uint16_t droppedEventCount(){
  return stats.dropped;
}

const EventQueueStats &eventQueueStats(){
  return stats;
}