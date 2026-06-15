#include "stopwatch.h"

#include "../core/system_state.h"

namespace StopwatchFeature {

void start(unsigned long now){
  SystemStateStore::enterStopwatchRunning(now);
}

void stop(unsigned long now){
  SystemStateStore::enterStopwatchPaused(now);
}

void resume(unsigned long now){
  SystemStateStore::enterStopwatchRunning(now);
}

void reset(){
  SystemStateStore::resetStopwatch();
}

void update(unsigned long now){
  SystemStateStore::updateStopwatch(now);
}

bool isRunning(){
  return SystemStateStore::current().stopwatch.state == StopwatchStateValue::RUNNING;
}

unsigned long elapsed(){
  return SystemStateStore::current().stopwatch.elapsed;
}

}