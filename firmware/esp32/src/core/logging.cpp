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
    case LogTag::LINK: return "LINK";
    case LogTag::SYNC: return "SYNC";
    case LogTag::STATE: return "STATE";
    case LogTag::HEARTBEAT: return "HEARTBEAT";
    case LogTag::PROTO: return "PROTO";
    case LogTag::SYSTEM: return "SYSTEM";
  }
  return "?";
}

}
