#include "timing.h"

namespace Timing {

bool intervalElapsed(unsigned long now, unsigned long &lastTick, unsigned long intervalMs){
  if(now-lastTick<=intervalMs) return false;
  lastTick=now;
  return true;
}

}
