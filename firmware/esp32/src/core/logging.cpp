#include "logging.h"

namespace Log {

bool shouldLog(RateLimitedLog &state, unsigned long intervalMs, unsigned long now){
  if(state.lastLogMs != 0 && now - state.lastLogMs < intervalMs) return false;
  state.lastLogMs = now;
  return true;
}

const char* tagName(LogTag tag){
  switch(tag){
    case LogTag::APP: return "APP";
    case LogTag::EVENTS: return "EVENTS";
    case LogTag::LED: return "LED";
  }
  return "?";
}

}
