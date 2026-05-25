#include "timer_service.h"

#include "../app/application_commands.h"
#include "../core/system_state.h"
#include "../features/timer.h"

namespace TimerService {

void update(uint32_t nowMs){
  TimerFeature::checkDone(nowMs);
}

void handleEvent(const AppEvent &event, uint32_t nowMs){
  if(event.type == EVENT_TIMER_DONE){
    SystemStateStore::startTimerAlarm(nowMs);
  }
}

}  // namespace TimerService
