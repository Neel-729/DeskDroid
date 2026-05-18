#include "application.h"

#include <esp_system.h>
#include <Arduino.h>

#include "navigation.h"
#include "settings_flow.h"
#include "../core/events.h"
#include "../core/settings_store.h"
#include "../core/timing.h"
#include "../drivers/buzzer_driver.h"
#include "../drivers/encoder_driver.h"
#include "../drivers/lcd_driver.h"
#include "../drivers/neopixel_driver.h"
#include "../drivers/rtc_driver.h"
#include "../features/clock.h"
#include "../features/lighting.h"
#include "../features/reminders.h"
#include "../features/stopwatch.h"
#include "../features/timer.h"
#include "../ui/screens.h"

namespace {

constexpr const char* FIRMWARE_VERSION = "2.5";

unsigned long lastDisplayRefresh = 0;
unsigned long lastLightScheduleCheck = 0;
unsigned long lastReminderCheck = 0;

bool blinkState = true;
unsigned long lastBlink = 0;

bool bootAnimation(){
  static uint8_t stage=0;
  static unsigned long lastStep=0;

  const char* frames[]={
    "Booting","Booting.","Booting..","Booting...","Booting....","Booting...","Booting....","Booting....."
  };

  if(stage==0){
    UiScreens::renderBootTitle(FIRMWARE_VERSION);
    BuzzerDriver::trigger();
    lastStep=millis();
    stage=1;
  }

  if(stage>=1 && stage<=8){
    if(millis()-lastStep>150){
      UiScreens::renderBootScreen(FIRMWARE_VERSION, frames[stage-1]);
      lastStep=millis();
      if(stage==6) BuzzerDriver::trigger(50);
      if(stage==8) BuzzerDriver::trigger(50);
      stage++;
    }
  }

  if(stage>8){
    stage=0;
    return true;
  }
  return false;
}

void initHardware(){
  Serial.begin(115200);
  LcdDriver::begin();
  LcdDriver::createBlockChar();

  if(!RtcDriver::begin()){
    UiScreens::renderRtcErrorScreen();
    while(true);
  }
  if(!RtcDriver::isRunning()){
    RtcDriver::adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  EncoderDriver::begin();
  BuzzerDriver::begin();

  SettingsStore::begin();
  SettingsFlow::begin();
  LightingFeature::refresh(SettingsFlow::settings());

  RemindersFeature::load();
  randomSeed(esp_random());

  NeoPixelDriver::begin(SettingsFlow::settings().ledBrightness, SettingsFlow::settings().idlePreset);
}

bool shouldBlinkCurrentScreen(){
  if(AppNavigation::current()==STATE_TIMER_EDIT) return true;
  if(AppNavigation::current()==STATE_TIMER_VIEW && TimerFeature::isRunning()) return true;
  if(AppNavigation::current()==STATE_REMINDER_EDIT) return true;
  if(AppNavigation::current()==STATE_SETTINGS_EDIT) return SettingsFlow::shouldBlink();
  return false;
}

void updateBlinkState(unsigned long now){
  if(!shouldBlinkCurrentScreen()) return;
  if(now-lastBlink>500){
    blinkState=!blinkState;
    lastBlink=now;
  }
}

void startReminderAlarm(uint8_t idx){
  AppNavigation::setResumeAfterReminder(AppNavigation::current());
  NeoPixelDriver::setState(LED_REMINDER_ALARM);
  RemindersFeature::startAlarm(idx);
  AppNavigation::enter(STATE_REMINDER_ALARM);
}

void stopReminderAlarm(){
  AppState resumeState = AppNavigation::resumeAfterReminder();
  RemindersFeature::stopAlarm();
  NeoPixelDriver::setState(resumeState==STATE_TIMER_ALARM ? LED_TIMER_ALARM : LED_IDLE);
  AppNavigation::enter(resumeState);
}

void checkReminders(){
  RemindersFeature::check(AppNavigation::current()==STATE_REMINDER_ALARM);
}

void updateReminderAlarm(){
  if(AppNavigation::current()!=STATE_REMINDER_ALARM) return;
  RemindersFeature::updateAlarm();
}

void resetTimerAlarm(bool restoreDuration){
  NeoPixelDriver::setState(LED_IDLE);
  TimerFeature::stopAlarm(restoreDuration);
  AppNavigation::enter(STATE_TIMER_VIEW);
}

void checkTimerDone(){
  if(AppNavigation::current()==STATE_TIMER_ALARM) return;
  TimerFeature::checkDone();
}

void handleEvent(const AppEvent &event){
  EventType ev = event.type;
  DeviceSettings &settings = SettingsFlow::settings();

  switch(ev){
    case EVENT_TIMER_DONE:
      NeoPixelDriver::setState(LED_TIMER_ALARM);
      TimerFeature::startAlarm(millis());
      AppNavigation::enter(STATE_TIMER_ALARM);
      return;

    case EVENT_TIMER_ALARM_TIMEOUT:
      resetTimerAlarm(false);
      return;

    case EVENT_REMINDER_TRIGGER:
      startReminderAlarm(event.data);
      return;

    case EVENT_REMINDER_TIMEOUT:
      stopReminderAlarm();
      return;

    default:
      break;
  }

  if(AppNavigation::current()==STATE_REMINDER_ALARM){
    if(ev==EVENT_CLICK || ev==EVENT_DOUBLE_CLICK || ev==EVENT_LONG_PRESS){
      stopReminderAlarm();
    }
    return;
  }

  if(ev==EVENT_ROTATE_CW || ev==EVENT_ROTATE_CCW){
    int step = (ev==EVENT_ROTATE_CW)?1:-1;

    switch(AppNavigation::current()){
      case STATE_REMINDER_LIST:
        RemindersFeature::rotateSelected(step);
        AppNavigation::markChanged();
        if(settings.buzzer) BuzzerDriver::trigger(20);
        break;

      case STATE_REMINDER_EDIT:
        RemindersFeature::adjustSelected(step);
        RemindersFeature::save();
        AppNavigation::markChanged();
        if(settings.buzzer) BuzzerDriver::trigger(20);
        break;

      case STATE_TIMER_EDIT:
        TimerFeature::adjustEdit(step);
        AppNavigation::markChanged();
        break;

      case STATE_SETTINGS_MENU:
        SettingsFlow::rotateMenu(step);
        break;

      case STATE_SETTINGS_EDIT:
        SettingsFlow::adjustValue(step);
        break;

      case STATE_CLOCK:
      case STATE_TIMER_VIEW:
      case STATE_STOPWATCH:
      case STATE_REMINDER_HOME:
      case STATE_SETTINGS_HOME:
        AppNavigation::rotateMainState(step);
        if(settings.buzzer) BuzzerDriver::trigger(20);
        break;

      default:
        break;
    }
  }

  if(ev==EVENT_CLICK){
    switch(AppNavigation::current()){
      case STATE_CLOCK:
        ClockFeature::nextQuote();
        break;

      case STATE_TIMER_VIEW:
        if(TimerFeature::isRunning()){
          TimerFeature::pause(millis());
        } else {
          TimerFeature::start(millis());
        }
        break;

      case STATE_TIMER_EDIT:
        TimerFeature::advanceEditField();
        AppNavigation::markChanged();
        break;

      case STATE_TIMER_ALARM:
        resetTimerAlarm(true);
        break;

      case STATE_STOPWATCH:
        StopwatchFeature::toggle(millis());
        break;

      case STATE_REMINDER_HOME:
        AppNavigation::enter(STATE_REMINDER_LIST);
        break;

      case STATE_REMINDER_LIST:
        AppNavigation::enter(STATE_REMINDER_EDIT);
        break;

      case STATE_REMINDER_EDIT:
        RemindersFeature::advanceEditField();
        AppNavigation::markChanged();
        break;

      case STATE_SETTINGS_HOME:
        AppNavigation::enter(STATE_SETTINGS_MENU);
        break;

      case STATE_SETTINGS_MENU:
        SettingsFlow::beginEdit();
        break;

      case STATE_SETTINGS_EDIT:
        SettingsFlow::advanceField();
        break;

      default:
        break;
    }

    if(settings.buzzer) BuzzerDriver::trigger(40);
  }

  if(ev==EVENT_DOUBLE_CLICK){
    switch(AppNavigation::current()){
      case STATE_REMINDER_EDIT:
        RemindersFeature::save();
        AppNavigation::enter(STATE_REMINDER_LIST);
        break;

      case STATE_REMINDER_LIST:
        AppNavigation::enter(STATE_REMINDER_HOME);
        if(settings.buzzer) BuzzerDriver::trigger(80);
        break;

      case STATE_SETTINGS_EDIT:
        SettingsFlow::commitEdit();
        break;

      case STATE_SETTINGS_MENU:
        SettingsFlow::leaveMenu();
        break;

      case STATE_SETTINGS_HOME:
        SettingsFlow::exitToClock();
        break;

      case STATE_TIMER_VIEW:
      case STATE_TIMER_EDIT:
      case STATE_TIMER_ALARM:
        TimerFeature::reset();
        resetTimerAlarm(false);
        break;

      case STATE_STOPWATCH:
        StopwatchFeature::reset();
        if(settings.buzzer) BuzzerDriver::trigger(120);
        break;

      default:
        break;
    }
  }

  if(ev==EVENT_LONG_PRESS){
    switch(AppNavigation::current()){
      case STATE_REMINDER_LIST:
        RemindersFeature::toggleSelectedActive();
        RemindersFeature::save();
        AppNavigation::markChanged();
        break;

      case STATE_TIMER_VIEW:
        if(!TimerFeature::isRunning()) AppNavigation::enter(STATE_TIMER_EDIT);
        break;

      case STATE_TIMER_EDIT:
        AppNavigation::enter(STATE_TIMER_VIEW);
        break;

      case STATE_TIMER_ALARM:
        resetTimerAlarm(true);
        break;

      case STATE_SETTINGS_EDIT:
        SettingsFlow::commitEdit();
        SettingsFlow::exitToClock();
        break;

      case STATE_SETTINGS_MENU:
      case STATE_SETTINGS_HOME:
        SettingsFlow::save();
        LightingFeature::refresh(settings);
        SettingsFlow::exitToClock();
        break;

      default:
        break;
    }
    if(settings.buzzer) BuzzerDriver::trigger(120);
  }
}

void processEvents(){
  AppEvent event;
  while(dequeueEvent(event)){
    handleEvent(event);
  }
}

}

namespace Application {

void setup(){
  initHardware();
  TimerFeature::begin();

  while(!bootAnimation()){ BuzzerDriver::update(); }

  ClockFeature::begin();
  LcdDriver::clear();
}

void loop(){
  unsigned long now=millis();
  DeviceSettings &settings = SettingsFlow::settings();

  BuzzerDriver::update();

  if(Timing::intervalElapsed(now,lastLightScheduleCheck,1000)){
    LightingFeature::refresh(settings);
  }

  NeoPixelDriver::update(LightingFeature::allowsOutput());

  if(AppNavigation::current()==STATE_TIMER_ALARM && TimerFeature::shouldAlarmBeep(now)){
    BuzzerDriver::trigger(200);
  }
  if(AppNavigation::current()==STATE_TIMER_ALARM && TimerFeature::alarmTimedOut(now)){
    enqueueEvent(EVENT_TIMER_ALARM_TIMEOUT);
  }

  checkTimerDone();

  if(Timing::intervalElapsed(now,lastReminderCheck,1000)){
    checkReminders();
  }

  if(AppNavigation::current()==STATE_REMINDER_ALARM){
    updateReminderAlarm();
  }

  EventType inputEvent=EncoderDriver::readEvent();
  if(inputEvent!=EVENT_NONE){
    enqueueEvent(inputEvent);
  }

  processEvents();

  if(now-lastDisplayRefresh>250 || AppNavigation::hasStateChanged()){
    if(AppNavigation::hasStateChanged()){
      LcdDriver::clear();
      AppNavigation::clearStateChanged();
    }
    updateBlinkState(now);

    switch(AppNavigation::current()){
      case STATE_CLOCK:
        ClockFeature::update(now,settings.quotes,settings.format24);
        UiScreens::renderClockScreen(ClockFeature::timeRow(),ClockFeature::quoteRow());
        break;

      case STATE_TIMER_VIEW:
      case STATE_TIMER_EDIT:
      case STATE_TIMER_ALARM:
        UiScreens::renderTimerScreen(AppNavigation::current(),now,blinkState);
        break;

      case STATE_STOPWATCH:
        StopwatchFeature::update(now);
        UiScreens::renderStopwatchScreen();
        break;

      case STATE_REMINDER_HOME:
      case STATE_REMINDER_LIST:
      case STATE_REMINDER_EDIT:
        UiScreens::renderRemindersScreen(AppNavigation::current(),settings.format24,blinkState);
        break;

      case STATE_SETTINGS_HOME:
      case STATE_SETTINGS_MENU:
      case STATE_SETTINGS_EDIT:
        UiScreens::renderSettingsScreen(AppNavigation::current(),settings,SettingsFlow::snapshot(FIRMWARE_VERSION,blinkState));
        break;

      case STATE_REMINDER_ALARM:
        UiScreens::renderReminderAlarmScreen();
        break;
    }

    lastDisplayRefresh=now;
  }
}

}
