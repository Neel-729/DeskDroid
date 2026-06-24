#include "timer.h"

#include "../core/events.h"

namespace {

uint8_t adjustedFieldValue(uint8_t value, int step, uint8_t maxValue){
  int adjusted = static_cast<int>(value) + step;
  if(adjusted < 0) adjusted = 0;
  if(adjusted > maxValue) adjusted = maxValue;
  return static_cast<uint8_t>(adjusted);
}

}  // namespace

namespace TimerFeature {

void begin(){
  const TimerState &timer = SystemStateStore::current().timer;
  SystemStateStore::setTimerClockFields(timer.hours, timer.minutes, timer.seconds);
}

void enterEditing(){
  SystemStateStore::enterTimerEditing();
}

void exitEditing(){
  SystemStateStore::exitTimerEditing();
}

void enterRunning(unsigned long now){
  SystemStateStore::enterTimerRunning(now);
}

void enterPaused(unsigned long now){
  SystemStateStore::enterTimerPaused(now);
}

void enterComplete(unsigned long now){
  SystemStateStore::enterTimerComplete(now);
}

void confirmReset(){
  SystemStateStore::confirmTimerReset();
}

void update(unsigned long now){
  const TimerState &timer = SystemStateStore::current().timer;
  if(timer.state != TimerStateValue::RUNNING) return;

  if(now >= timer.endsAtMs){
    enqueueTimerEvent(EVENT_TIMER_DONE);
  } else {
    SystemStateStore::updateTimer(now);
  }
}

bool isRunning(){
  return SystemStateStore::current().timer.state == TimerStateValue::RUNNING;
}

bool isPaused(){
  return SystemStateStore::current().timer.state == TimerStateValue::PAUSED;
}

bool isEditing(){
  return SystemStateStore::current().timer.state == TimerStateValue::EDITING;
}

bool isComplete(){
  return SystemStateStore::current().timer.state == TimerStateValue::COMPLETE;
}

bool isIdle(){
  return SystemStateStore::current().timer.state == TimerStateValue::IDLE;
}

unsigned long remainingMillis(unsigned long now){
  const TimerState &timer = SystemStateStore::current().timer;
  if(timer.state == TimerStateValue::RUNNING){
    return now >= timer.endsAtMs ? 0 : timer.endsAtMs - now;
  }
  return timer.remainingMs;
}

unsigned long totalMillis(){
  return SystemStateStore::current().timer.durationMs;
}

uint8_t hours(){
  return SystemStateStore::current().timer.hours;
}

uint8_t minutes(){
  return SystemStateStore::current().timer.minutes;
}

uint8_t seconds(){
  return SystemStateStore::current().timer.seconds;
}



void adjustEdit(int step){
  const TimerState &timer = SystemStateStore::current().timer;
  uint8_t nextHours = timer.hours;
  uint8_t nextMinutes = timer.minutes;
  uint8_t nextSeconds = timer.seconds;

  if(timer.editField == EDIT_HOURS){
    nextHours = adjustedFieldValue(timer.hours, step, 99);
  } else if(timer.editField == EDIT_MINUTES){
    nextMinutes = adjustedFieldValue(timer.minutes, step, 59);
  } else {
    nextSeconds = adjustedFieldValue(timer.seconds, step, 59);
  }

  SystemStateStore::setTimerClockFields(nextHours, nextMinutes, nextSeconds);
}

void advanceEditField(){
  const TimerEditField field = SystemStateStore::current().timer.editField;
  if(field == EDIT_HOURS) SystemStateStore::setTimerEditField(EDIT_MINUTES);
  else if(field == EDIT_MINUTES) SystemStateStore::setTimerEditField(EDIT_SECONDS);
  else SystemStateStore::setTimerEditField(EDIT_HOURS);
}

TimerEditField editField(){
  return SystemStateStore::current().timer.editField;
}

bool shouldAlarmBeep(unsigned long now){
  const TimerState &timer = SystemStateStore::current().timer;
  if(now - timer.lastAlarmBeepMs <= 1200) return false;
  // This is a read-only function, so we can't update the state here.
  // The state will be updated in the application layer.
  return true;
}

bool alarmTimedOut(unsigned long now){
  return now - SystemStateStore::current().timer.alarmStartedAtMs > 30000;
}

}