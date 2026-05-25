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

void start(unsigned long now){
  SystemStateStore::startTimer(now);
}

void pause(unsigned long now){
  SystemStateStore::pauseTimer(now);
}

void reset(){
  SystemStateStore::resetTimer();
}

void checkDone(unsigned long now){
  const TimerState &timer = SystemStateStore::current().timer;
  if(!timer.active) return;
  if(now < timer.endsAtMs) {
    SystemStateStore::updateTimerRemaining(now);
    return;
  }

  if(enqueueTimerEvent(EVENT_TIMER_DONE)){
    SystemStateStore::completeTimer(now);
  }
}

bool isRunning(){
  return SystemStateStore::current().timer.active;
}

unsigned long remainingMillis(unsigned long now){
  const TimerState &timer = SystemStateStore::current().timer;
  if(timer.active){
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

void startAlarm(unsigned long now){
  SystemStateStore::startTimerAlarm(now);
}

void stopAlarm(bool restoreDuration){
  SystemStateStore::stopTimerAlarm(restoreDuration);
}

bool shouldAlarmBeep(unsigned long now){
  const TimerState &timer = SystemStateStore::current().timer;
  if(now - timer.lastAlarmBeepMs <= 1200) return false;
  SystemStateStore::markTimerAlarmBeep(now);
  return true;
}

bool alarmTimedOut(unsigned long now){
  return now - SystemStateStore::current().timer.alarmStartedAtMs > 30000;
}

}
