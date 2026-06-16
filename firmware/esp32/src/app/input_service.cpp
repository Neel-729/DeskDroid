#include "input_service.h"
#include <Arduino.h>

#include "../drivers/encoder_driver.h"

namespace InputService {

void begin(){
  EncoderDriver::begin();
}

EventType readEvent(){
  EventType event = EncoderDriver::readEvent();
  // STAGE 2: Log events received from encoder driver
  if(event != EVENT_NONE){
    Serial.printf("[INPUT] event=%d\n", event);
  }
  return event;
}

}