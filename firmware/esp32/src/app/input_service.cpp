#include "input_service.h"
#include <Arduino.h>

// Compile-time debug toggle for input FSM logging (matches encoder_driver.cpp)
#ifndef INPUT_DEBUG
#define INPUT_DEBUG 0
#endif
#if INPUT_DEBUG
#define INPUT_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define INPUT_LOG(...)
#endif

#include "../drivers/encoder_driver.h"

namespace InputService {

void begin(){
  EncoderDriver::begin();
}

EventType readEvent(){
  EventType event = EncoderDriver::readEvent();
  // STAGE 2: Log events received from encoder driver (only when INPUT_DEBUG is enabled)
  if(event != EVENT_NONE){
    INPUT_LOG("[INPUT] event=%d\n", event);
  }
  return event;
}

}