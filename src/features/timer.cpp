#include "timer.h"

#include "../core/events.h"

namespace {
int timerHours=0;
int timerMinutes=5;
int timerSeconds=0;

unsigned long timerEndTime=0;
unsigned long timerRemainingMillis=0;
unsigned long timerTotalMillis=0;

bool timerRunning=false;

unsigned long lastAlarmBeep=0;
unsigned long alarmStartTime=0;

TimerEditField timerEditField=EDIT_MINUTES;

void recalculateTotal(){
  timerTotalMillis=((unsigned long)timerHours*3600UL+(unsigned long)timerMinutes*60UL+(unsigned long)timerSeconds)*1000UL;
  if(timerTotalMillis==0) timerTotalMillis=1000;
}
}

namespace TimerFeature {

void begin(){
  recalculateTotal();
  timerRemainingMillis=timerTotalMillis;
}

void start(unsigned long now){
  timerEndTime=now+timerRemainingMillis;
  timerRunning=true;
}

void pause(unsigned long now){
  timerRemainingMillis=(now>=timerEndTime)?0:timerEndTime-now;
  timerRunning=false;
}

void reset(){
  timerRunning=false;
  timerRemainingMillis=timerTotalMillis;
}

void checkDone(){
  if(!timerRunning) return;
  if(millis()<timerEndTime) return;

  if(enqueueEvent(EVENT_TIMER_DONE)){
    timerRunning=false;
    timerRemainingMillis=0;
  }
}

bool isRunning(){
  return timerRunning;
}

unsigned long remainingMillis(unsigned long now){
  if(timerRunning){
    return (now>=timerEndTime)?0:timerEndTime-now;
  }
  return timerRemainingMillis;
}

unsigned long totalMillis(){
  return timerTotalMillis;
}

uint8_t hours(){
  return timerHours;
}

uint8_t minutes(){
  return timerMinutes;
}

uint8_t seconds(){
  return timerSeconds;
}

void adjustEdit(int step){
  if(timerEditField==EDIT_HOURS) timerHours = constrain(timerHours + step,0,99);
  else if(timerEditField==EDIT_MINUTES) timerMinutes = constrain(timerMinutes + step,0,59);
  else timerSeconds = constrain(timerSeconds + step,0,59);

  recalculateTotal();
  timerRemainingMillis=timerTotalMillis;
}

void advanceEditField(){
  if(timerEditField==EDIT_HOURS) timerEditField=EDIT_MINUTES;
  else if(timerEditField==EDIT_MINUTES) timerEditField=EDIT_SECONDS;
  else timerEditField=EDIT_HOURS;
}

TimerEditField editField(){
  return timerEditField;
}

void startAlarm(unsigned long now){
  alarmStartTime=now;
  lastAlarmBeep=0;
}

void stopAlarm(bool restoreDuration){
  if(restoreDuration) timerRemainingMillis=timerTotalMillis;
}

bool shouldAlarmBeep(unsigned long now){
  if(now-lastAlarmBeep<=1200) return false;
  lastAlarmBeep=now;
  return true;
}

bool alarmTimedOut(unsigned long now){
  return now-alarmStartTime>30000;
}

}

