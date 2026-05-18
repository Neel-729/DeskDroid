#include "input_service.h"

#include "../drivers/encoder_driver.h"

namespace InputService {

void begin(){
  EncoderDriver::begin();
}

EventType readEvent(){
  return EncoderDriver::readEvent();
}

}
