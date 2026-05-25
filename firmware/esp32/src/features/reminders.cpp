#include "reminders.h"

#include "../core/events.h"
#include "../core/settings_store.h"
#include "../core/time_service.h"

namespace {
ReminderRecord reminders[RemindersFeature::MAX_REMINDERS];

uint8_t reminderIndex = 0;
ReminderEditField reminderField = REM_EDIT_HOUR;

uint8_t activeReminder = 255;
unsigned long reminderAlarmStart = 0;
unsigned long lastReminderBeep = 0;
uint8_t reminderBeepStage = 0;
uint32_t lastReminderTriggerStamp = 0xFFFFFFFF;
}

namespace RemindersFeature {

void load(){
  SettingsStore::loadReminders(reminders, MAX_REMINDERS);
}

void save(){
  SettingsStore::saveReminders(reminders, MAX_REMINDERS);
}

void check(bool alarmActive){
  if(alarmActive) return;

  DateTime now = TimeService::now();
  uint32_t minuteStamp = now.unixtime() / 60UL;
  if(minuteStamp == lastReminderTriggerStamp) return;

  for(uint8_t i=0;i<MAX_REMINDERS;i++){
    if(!reminders[i].active) continue;
    if(now.hour()==reminders[i].hour && now.minute()==reminders[i].minute){
      if(enqueueReminderEvent(EVENT_REMINDER_TRIGGER, i)){
        lastReminderTriggerStamp = minuteStamp;
      }
      break;
    }
  }
}

uint8_t selectedIndex(){
  return reminderIndex;
}

void rotateSelected(int step){
  int newIndex = (int)reminderIndex + step;
  if(newIndex < 0) newIndex = MAX_REMINDERS - 1;
  if(newIndex >= MAX_REMINDERS) newIndex = 0;
  reminderIndex = (uint8_t)newIndex;
}

ReminderEditField editField(){
  return reminderField;
}

void advanceEditField(){
  reminderField = (reminderField == REM_EDIT_HOUR) ? REM_EDIT_MINUTE : REM_EDIT_HOUR;
}

void adjustSelected(int step){
  if(reminderField==REM_EDIT_HOUR){
    int h = (int)reminders[reminderIndex].hour + step;
    if(h < 0) h = 23;
    if(h > 23) h = 0;
    reminders[reminderIndex].hour = (uint8_t)h;
  } else {
    int m = (int)reminders[reminderIndex].minute + step;
    if(m < 0) m = 59;
    if(m > 59) m = 0;
    reminders[reminderIndex].minute = (uint8_t)m;
  }
}

void toggleSelectedActive(){
  reminders[reminderIndex].active = !reminders[reminderIndex].active;
}

bool setReminder(uint8_t index, uint8_t hour, uint8_t minute, bool active){
  if(index >= MAX_REMINDERS || hour > 23 || minute > 59) return false;
  reminders[index].hour = hour;
  reminders[index].minute = minute;
  reminders[index].active = active;
  return true;
}

bool selectedActive(){
  return reminders[reminderIndex].active;
}

uint8_t selectedHour(){
  return reminders[reminderIndex].hour;
}

uint8_t selectedMinute(){
  return reminders[reminderIndex].minute;
}

bool getNext(uint8_t &hour, uint8_t &minute){
  DateTime now = TimeService::now();

  int nowMinutes = now.hour()*60 + now.minute();
  int bestDiff = 1440;
  bool found = false;

  for(uint8_t i=0;i<MAX_REMINDERS;i++){
    if(!reminders[i].active) continue;

    int remMinutes = reminders[i].hour*60 + reminders[i].minute;
    int diff = remMinutes - nowMinutes;

    if(diff < 0) diff += 1440;

    if(diff < bestDiff){
      bestDiff = diff;
      hour = reminders[i].hour;
      minute = reminders[i].minute;
      found = true;
    }
  }

  return found;
}

void startAlarm(uint8_t idx, unsigned long now){
  activeReminder = idx;
  reminderAlarmStart = now;
  reminderBeepStage = 0;
  lastReminderBeep = 0;
}

void stopAlarm(){
  activeReminder = 255;
}

bool updateAlarm(unsigned long now){
  if(activeReminder>=MAX_REMINDERS) return false;

  if(now - reminderAlarmStart > 60000){
    enqueueReminderEvent(EVENT_REMINDER_TIMEOUT, activeReminder);
    return false;
  }

  bool shouldBeep = false;
  if(now - lastReminderBeep > 120){
    uint8_t stage = reminderBeepStage % 8;
    if(stage==0 || stage==2 || stage==4){
      shouldBeep = true;
    }
    reminderBeepStage++;
    if(reminderBeepStage >= 8){
      reminderBeepStage = 0;
      lastReminderBeep = now + 400;
    } else {
      lastReminderBeep = now;
    }
  }
  return shouldBeep;
}

uint8_t activeAlarmIndex(){
  return activeReminder;
}

uint8_t activeAlarmHour(){
  if(activeReminder>=MAX_REMINDERS) return 0;
  return reminders[activeReminder].hour;
}

uint8_t activeAlarmMinute(){
  if(activeReminder>=MAX_REMINDERS) return 0;
  return reminders[activeReminder].minute;
}

}
