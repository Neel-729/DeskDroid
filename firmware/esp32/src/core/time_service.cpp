#include "time_service.h"

#include "../drivers/rtc_driver.h"

namespace TimeService {

bool begin(){
  return RtcDriver::begin();
}

bool isRunning(){
  return RtcDriver::isRunning();
}

DateTime now(){
  return RtcDriver::now();
}

void adjust(const DateTime &value){
  RtcDriver::adjust(value);
}

}
