#include "rtc_driver.h"

namespace {
RTC_DS1307 rtc;
}

namespace RtcDriver {

bool begin(){
  return rtc.begin();
}

bool isRunning(){
  return rtc.isrunning();
}

DateTime now(){
  return rtc.now();
}

void adjust(const DateTime &value){
  rtc.adjust(value);
}

}

