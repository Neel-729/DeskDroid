#include "time_service.h"

#include <Arduino.h>

#include "../drivers/rtc_driver.h"

namespace {
bool rtcAvailable = false;
DateTime fallbackBase(F(__DATE__), F(__TIME__));
uint32_t fallbackBaseMs = 0;

DateTime fallbackNow(){
  const uint32_t elapsedSeconds = (millis() - fallbackBaseMs) / 1000UL;
  return DateTime(fallbackBase.unixtime() + elapsedSeconds);
}
}

namespace TimeService {

bool begin(){
  fallbackBase = DateTime(F(__DATE__), F(__TIME__));
  fallbackBaseMs = millis();
  rtcAvailable = RtcDriver::begin();
  return true;
}

bool isRunning(){
  return rtcAvailable && RtcDriver::isRunning();
}

DateTime now(){
  if(rtcAvailable){
    return RtcDriver::now();
  }
  return fallbackNow();
}

void adjust(const DateTime &value){
  fallbackBase = value;
  fallbackBaseMs = millis();
  if(rtcAvailable){
    RtcDriver::adjust(value);
  }
}

}
