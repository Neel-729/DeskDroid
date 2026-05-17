#include "events.h"

namespace {
constexpr uint8_t EVENT_QUEUE_SIZE = 16;
AppEvent eventQueue[EVENT_QUEUE_SIZE];
uint8_t eventQueueHead = 0;
uint8_t eventQueueTail = 0;
uint16_t droppedEvents = 0;
}

bool enqueueEvent(EventType type, uint8_t data){
  if(type==EVENT_NONE) return true;

  uint8_t nextHead = (eventQueueHead + 1) % EVENT_QUEUE_SIZE;
  if(nextHead == eventQueueTail){
    droppedEvents++;
    return false;
  }

  eventQueue[eventQueueHead] = { type, data };
  eventQueueHead = nextHead;
  return true;
}

bool dequeueEvent(AppEvent &event){
  if(eventQueueTail == eventQueueHead) return false;

  event = eventQueue[eventQueueTail];
  eventQueueTail = (eventQueueTail + 1) % EVENT_QUEUE_SIZE;
  return true;
}

uint16_t droppedEventCount(){
  return droppedEvents;
}

