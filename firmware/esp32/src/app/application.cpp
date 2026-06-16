#include "application.h"

#include <esp_system.h>
#include <Arduino.h>

#include "application_commands.h"
#include "hardware_requests.h"
#include "idle_manager.h"
#include "input_service.h"
#include "navigation.h"
#include "navigation_stack.h"
#include "settings_flow.h"
#include "system_context.h"
#include "../core/events.h"
#include "../core/fault_tracker.h"
#include "../core/logging.h"
#include "../core/persistent_storage.h"
#include "../core/scheduler.h"
#include "../core/settings_store.h"
#include "../core/system_state.h"
#include "../core/time_service.h"
#include "../drivers/lcd_driver.h"
#include "../features/clock.h"
#include "../features/reminders.h"
#include "../features/stopwatch.h"
#include "../features/timer.h"
#include "../protocol/esp8266_link.h"
#include "../protocol/uart_monitor.h"
#include "../services/audio_service.h"
#include "../services/lighting_service.h"
#include "../services/services.h"
#include "../services/timer_service.h"
#include "../ui/screens.h"

namespace {

constexpr const char* FIRMWARE_VERSION = "2.6.6";

SystemContext systemContext;

void runBuzzerTask(FrameContext &context);
void runEsp8266LinkTask(FrameContext &context);
void runHardwareRequestTask(FrameContext &context);
void runLightingTask(FrameContext &context);
void runTimerTask(FrameContext &context);
void runReminderCheckTask(FrameContext &context);
void runReminderAlarmTask(FrameContext &context);
void runNavMonitorTask(FrameContext &context);
void runInputTask(FrameContext &context);
void runEventTask(FrameContext &context);
void runUiTask(FrameContext &context);
void runDiagnosticsTask(FrameContext &context);
void monitorLongPressHome(unsigned long now);
void flushUiFrame();
bool flushUiFrame(bool force);
UiScreens::TimerScreenData timerScreenData(unsigned long now);
UiScreens::StopwatchScreenData stopwatchScreenData();
UiScreens::RemindersScreenData remindersScreenData();
void logTaskRuntimeProfiles();
void logTaskRuntimeRanking(uint8_t metric);
void logOverrunAttributionSummary();

ScheduledTask scheduledTasks[] = {
  { "esp8266-link", 0, 0, 0, 1200, 0, 0, true, runEsp8266LinkTask },
  { "buzzer", 5, 0, 0, 250, 0, 0, true, runBuzzerTask },
  { "hardware", 0, 0, 0, 1500, 0, 0, true, runHardwareRequestTask },
  { "lighting", 1000, 0, 0, 1500, 0, 0, true, runLightingTask },
  { "timer", 50, 0, 0, 1000, 0, 0, true, runTimerTask },
  { "reminder-check", 1000, 0, 0, 2500, 0, 0, true, runReminderCheckTask },
  { "reminder-alarm", 50, 0, 0, 1000, 0, 0, true, runReminderAlarmTask },
  { "nav-monitor", 0, 0, 0, 50, 0, 0, true, runNavMonitorTask },
  { "input", 10, 0, 0, 1000, 0, 0, true, runInputTask },
  { "events", 0, 0, 0, 2500, 0, 0, true, runEventTask },
  { "ui", 50, 0, 0, 12000, 0, 0, true, runUiTask },
  { "diagnostics", 30000, 0, 0, 4000, 0, 0, true, runDiagnosticsTask }
};

Scheduler scheduler(scheduledTasks, sizeof(scheduledTasks) / sizeof(scheduledTasks[0]));

struct UiTaskTimingStats {
  uint32_t decisionUs = 0;
  uint32_t dataUs = 0;
  uint32_t menuDrawingUs = 0;
  uint32_t textRenderingUs = 0;
  uint32_t iconRenderingUs = 0;
  uint32_t animationRenderingUs = 0;
  uint32_t displayRenderingUs = 0;
  uint32_t spiTransferUs = 0;
  uint32_t frameUs = 0;
  uint32_t maxFrameUs = 0;
  uint32_t framesBuilt = 0;
  uint32_t framesFlushed = 0;
  uint32_t framesSkipped = 0;
  uint32_t unchangedFrames = 0;
  uint32_t fullScreenRedraws = 0;
  uint32_t unnecessaryRedraws = 0;
};

UiTaskTimingStats uiTiming;
char lastFlushedRows[2][17] = {
  "                ",
  "                "
};
bool lastUiFrameValid = false;

bool bootAnimation(unsigned long now){
  static uint8_t stage=0;
  static unsigned long lastStep=0;

  const char* frames[]={
    "Booting","Booting.","Booting..","Booting...","Booting....","Booting...","Booting....","Booting....."
  };

  if(stage==0){
    UiScreens::renderBootTitle(FIRMWARE_VERSION);
    AudioService::beep(50);
    HardwareRequests::executePending();
    lastStep=now;
    stage=1;
  }

  if(stage>=1 && stage<=8){
    if(now-lastStep>150){
      UiScreens::renderBootScreen(FIRMWARE_VERSION, frames[stage-1]);
      lastStep=now;
      if(stage==6) AudioService::beep(50);
      if(stage==8) AudioService::beep(50);
      HardwareRequests::executePending();
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
  FaultTracker::begin();
  SettingsStore::begin();
  SettingsFlow::begin();
  SystemStateStore::begin(SettingsFlow::settings());
  
  // Initialize navigation stack
  NavigationStack::begin();
  
  // Initialize idle manager with timeout from settings
  IdleManager::begin();
  IdleManager::setIdleTimeout((IdleManager::IdleTimeout)SettingsFlow::settings().idleTimeoutSeconds);
  
  // Initialize persistent storage and restore relay states
  PersistentStorage::begin();
  bool savedRelayStates[SystemState::RelayCount] = {};
  if(PersistentStorage::loadRelayStates(savedRelayStates, SystemState::RelayCount)){
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] Restoring relay states from NVS");
    SystemStateStore::restoreRelayStates(savedRelayStates, SystemState::RelayCount);
  } else {
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] No saved relay state found, using defaults");
  }
  
  HardwareRequests::beginLocal(SettingsFlow::settings());

  if(!TimeService::begin()){
    UiScreens::renderRtcErrorScreen();
    flushUiFrame();
    while(true){
      delay(100);
    }
  }
  if(!TimeService::isRunning()){
    TimeService::adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Services::begin(SettingsFlow::settings());
  InputService::begin();
  HardwareRequests::requestBacklight(SystemStateStore::current().lighting.backlightEnabled, CommandSource::SYSTEM);
  HardwareRequests::executePending();

  RemindersFeature::load();
  randomSeed(esp_random());

}

bool shouldBlinkCurrentScreen(){
  if(AppNavigation::current()==STATE_TIMER_EDIT) return true;
  if(AppNavigation::current()==STATE_TIMER_VIEW && TimerFeature::isRunning()) return true;
  if(AppNavigation::current()==STATE_REMINDER_EDIT) return true;
  if(AppNavigation::current()==STATE_SETTINGS_EDIT) return SettingsFlow::shouldBlink();
  return false;
}

bool blinkDue(unsigned long now){
  return shouldBlinkCurrentScreen() && now - systemContext.ui.lastBlinkMs > 500;
}

bool screenNeedsPeriodicRefresh(AppState state){
  switch(state){
    case STATE_CLOCK:
      return true;

    case STATE_TIMER_VIEW:
      return TimerFeature::isRunning();

    case STATE_TIMER_EDIT:
    case STATE_REMINDER_EDIT:
      return shouldBlinkCurrentScreen();

    case STATE_STOPWATCH:
      return StopwatchFeature::isRunning();

    case STATE_SETTINGS_EDIT:
      return SettingsFlow::shouldBlink();

    default:
      return false;
  }
}

bool shouldBuildUiFrame(unsigned long now, bool stateChanged){
  if(stateChanged) return true;
  if(blinkDue(now)) return true;
  if(!screenNeedsPeriodicRefresh(AppNavigation::current())) return false;
  return now - systemContext.ui.lastRefreshMs > 250;
}

bool updateBlinkState(unsigned long now){
  if(!shouldBlinkCurrentScreen()) return false;
  if(now-systemContext.ui.lastBlinkMs>500){
    systemContext.ui.blinkState=!systemContext.ui.blinkState;
    systemContext.ui.lastBlinkMs=now;
    return true;
  }
  return false;
}

void startReminderAlarm(uint8_t idx, unsigned long now){
  AppNavigation::setResumeAfterReminder(AppNavigation::current());
  AppCommands::setLedMode(LED_REMINDER_ALARM);
  RemindersFeature::startAlarm(idx, now);
  AppNavigation::enter(STATE_REMINDER_ALARM);
}

void stopReminderAlarm(){
  AppState resumeState = AppNavigation::resumeAfterReminder();
  RemindersFeature::stopAlarm();
  AppCommands::setLedMode(resumeState==STATE_TIMER_ALARM ? LED_TIMER_ALARM : LED_IDLE);
  AppNavigation::enter(resumeState);
}

void checkReminders(){
  RemindersFeature::check(AppNavigation::current()==STATE_REMINDER_ALARM);
}

void updateReminderAlarm(unsigned long now){
  if(AppNavigation::current()!=STATE_REMINDER_ALARM) return;
  if(RemindersFeature::updateAlarm(now)){
    AudioService::beep(80);
  }
}

void resetTimerAlarm(bool restoreDuration){
  AppCommands::setLedMode(LED_IDLE);
  TimerFeature::confirmReset();
  AppNavigation::enter(STATE_TIMER_VIEW);
}

void checkTimerDone(unsigned long now){
  if(AppNavigation::current()==STATE_TIMER_ALARM) return;
  TimerFeature::update(now);
}

void handleEvent(const AppEvent &event, unsigned long now){
  EventType ev = event.type;
  DeviceSettings &settings = SettingsFlow::settings();

  // STAGE 5: Log application receiving the event
  if(ev == EVENT_CLICK || ev == EVENT_DOUBLE_CLICK || ev == EVENT_LONG_PRESS){
    Serial.printf("[APP] event=%d\n", ev);
  }

  // Track user activity for idle manager
  if(ev == EVENT_CLICK || ev == EVENT_DOUBLE_CLICK || ev == EVENT_LONG_PRESS ||
     ev == EVENT_ROTATE_CW || ev == EVENT_ROTATE_CCW){
    IdleManager::notifyActivity(now);
  }

  switch(ev){
    case EVENT_TIMER_DONE:
      AppCommands::setLedMode(LED_TIMER_ALARM);
      AppNavigation::enter(STATE_TIMER_ALARM);
      return;

    case EVENT_TIMER_ALARM_TIMEOUT:
      resetTimerAlarm(false);
      return;

    case EVENT_REMINDER_TRIGGER:
      startReminderAlarm(event.payload.reminder.index, now);
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

  // Handle long press to go home
  if(ev == EVENT_LONG_PRESS){
    LOG_INFO(LogTag::APP, "[NAV] Long press home triggered");
    AppNavigation::goHome();
    AudioService::beep(200);  // Distinct beep for long press home
    return;
  }

  if(ev==EVENT_ROTATE_CW || ev==EVENT_ROTATE_CCW){
    int step = event.payload.encoder.direction;
    if(step == 0) step = (ev==EVENT_ROTATE_CW)?1:-1;

    switch(AppNavigation::current()){
      case STATE_REMINDER_LIST:
        RemindersFeature::rotateSelected(step);
        AppNavigation::markChanged();
        AudioService::beep(20);
        break;

      case STATE_REMINDER_EDIT:
        RemindersFeature::adjustSelected(step);
        RemindersFeature::save();
        AppNavigation::markChanged();
        AudioService::beep(20);
        break;

      case STATE_TIMER_EDIT:
        AppCommands::adjustTimerEdit(step);
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
        AudioService::beep(20);
        break;

      default:
        break;
    }
  }

  if(ev==EVENT_CLICK){
    switch(AppNavigation::current()){
      case STATE_CLOCK:
        ClockFeature::nextQuote(now);
        break;

      case STATE_TIMER_VIEW:
        {
          const auto& timer = SystemStateStore::current().timer;
          if (timer.state == TimerStateValue::RUNNING) {
            TimerFeature::enterPaused(now);
          } else {
            TimerFeature::enterRunning(now);
          }
        }
        break;

      case STATE_TIMER_EDIT:
        AppCommands::advanceTimerEditField();
        AppNavigation::markChanged();
        break;

      case STATE_TIMER_ALARM:
        resetTimerAlarm(true);
        break;

      case STATE_STOPWATCH:
        {
          const auto& stopwatch = SystemStateStore::current().stopwatch;
          if (stopwatch.state == StopwatchStateValue::RUNNING) {
            StopwatchFeature::stop(now);
          } else if (stopwatch.state == StopwatchStateValue::PAUSED) {
            StopwatchFeature::resume(now);
          } else {
            StopwatchFeature::start(now);
          }
        }
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

    AudioService::beep(40);
  }

  if(ev==EVENT_DOUBLE_CLICK){
    switch(AppNavigation::current()){
      case STATE_REMINDER_EDIT:
        RemindersFeature::save();
        AppNavigation::enter(STATE_REMINDER_LIST);
        break;

      case STATE_REMINDER_LIST:
        AppNavigation::enter(STATE_REMINDER_HOME);
        AudioService::beep(80);
        break;

      case STATE_SETTINGS_EDIT:
        SettingsFlow::commitEdit(now);
        break;

      case STATE_SETTINGS_MENU:
        SettingsFlow::leaveMenu();
        break;

      case STATE_SETTINGS_HOME:
        SettingsFlow::exitToClock();
        break;

      case STATE_TIMER_VIEW:
        if (!TimerFeature::isRunning()) {
          TimerFeature::enterEditing();
        }
        break;

      case STATE_TIMER_EDIT:
        AppNavigation::enter(STATE_TIMER_VIEW);
        break;

      case STATE_STOPWATCH:
        AppNavigation::goHome();
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
        TimerFeature::confirmReset();
        break;

      case STATE_STOPWATCH:
        StopwatchFeature::reset();
        AudioService::beep(120);
        break;

      case STATE_TIMER_ALARM:
        resetTimerAlarm(true);
        break;

      case STATE_SETTINGS_EDIT:
        SettingsFlow::commitEdit(now);
        SettingsFlow::exitToClock();
        break;

      case STATE_SETTINGS_MENU:
      case STATE_SETTINGS_HOME:
        SettingsFlow::save();
        LightingService::refreshSchedule(settings);
        SettingsFlow::exitToClock();
        break;

      default:
        break;
    }
    AudioService::beep(120);
  }
}

bool processEvents(unsigned long now){
  bool processed = false;
  AppEvent event;
  while(dequeueEvent(event)){
    processed = true;
    Services::handleEvent(event, now);
    handleEvent(event, now);
  }
  return processed;
}

bool sameUiRows(const char* row0, const char* row1){
  return lastUiFrameValid &&
    strncmp(lastFlushedRows[0], row0, 16) == 0 &&
    strncmp(lastFlushedRows[1], row1, 16) == 0;
}

void rememberUiRows(const char* row0, const char* row1){
  strncpy(lastFlushedRows[0], row0, 16);
  strncpy(lastFlushedRows[1], row1, 16);
  lastFlushedRows[0][16] = '\0';
  lastFlushedRows[1][16] = '\0';
  lastUiFrameValid = true;
}

void flushUiFrame(){
  (void)flushUiFrame(false);
}

bool flushUiFrame(bool force){
  const char* row0 = UiScreens::row(0);
  const char* row1 = UiScreens::row(1);
  if(!force && sameUiRows(row0, row1)){
    uiTiming.unchangedFrames++;
    return false;
  }

  HardwareRequests::writeDisplayRows(row0, row1);
  rememberUiRows(row0, row1);
  uiTiming.framesFlushed++;
  return true;
}

UiScreens::TimerScreenData timerScreenData(unsigned long now){
  UiScreens::TimerScreenData data = {
    TimerFeature::isRunning(),
    TimerFeature::totalMillis(),
    TimerFeature::remainingMillis(now),
    TimerFeature::hours(),
    TimerFeature::minutes(),
    TimerFeature::seconds(),
    (uint8_t)TimerFeature::editField()
  };
  return data;
}

UiScreens::StopwatchScreenData stopwatchScreenData(){
  UiScreens::StopwatchScreenData data = {
    StopwatchFeature::elapsed()
  };
  return data;
}

UiScreens::RemindersScreenData remindersScreenData(){
  uint8_t nextHour = 0;
  uint8_t nextMinute = 0;
  bool hasNext = RemindersFeature::getNext(nextHour, nextMinute);

  UiScreens::RemindersScreenData data = {
    RemindersFeature::MAX_REMINDERS,
    RemindersFeature::selectedIndex(),
    RemindersFeature::selectedActive(),
    RemindersFeature::selectedHour(),
    RemindersFeature::selectedMinute(),
    (uint8_t)RemindersFeature::editField(),
    hasNext,
    nextHour,
    nextMinute,
    RemindersFeature::activeAlarmIndex(),
    RemindersFeature::activeAlarmHour(),
    RemindersFeature::activeAlarmMinute()
  };
  return data;
}

void runBuzzerTask(FrameContext &context){
  (void)context;
  AudioService::service();
}

void runEsp8266LinkTask(FrameContext &context){
  Services::update(context.nowMs);
}

void runHardwareRequestTask(FrameContext &context){
  (void)context;
  HardwareRequests::executePending();
}

void runLightingTask(FrameContext &context){
  (void)context;
  DeviceSettings &settings = SettingsFlow::settings();
  LightingService::refreshSchedule(settings);
}

void runTimerTask(FrameContext &context){
  const unsigned long now = context.nowMs;

  if(AppNavigation::current()==STATE_TIMER_ALARM && TimerFeature::shouldAlarmBeep(now)){
    AudioService::beep(200);
  }
  if(AppNavigation::current()==STATE_TIMER_ALARM && TimerFeature::alarmTimedOut(now)){
    enqueueTimerEvent(EVENT_TIMER_ALARM_TIMEOUT);
  }

  TimerService::update(now);
}

void runReminderCheckTask(FrameContext &context){
  (void)context;
  checkReminders();
}

void runReminderAlarmTask(FrameContext &context){
  updateReminderAlarm(context.nowMs);
}

void runNavMonitorTask(FrameContext &context){
  // Navigation monitoring removed - button handling now centralized in EncoderDriver
}

void runInputTask(FrameContext &context){
  (void)context;
  EventType inputEvent=InputService::readEvent();
  if(inputEvent!=EVENT_NONE){
    int8_t direction = 0;
    if(inputEvent==EVENT_ROTATE_CW) direction = 1;
    else if(inputEvent==EVENT_ROTATE_CCW) direction = -1;
    enqueueEncoderEvent(inputEvent, direction);
  }
}

void runEventTask(FrameContext &context){
  if(processEvents(context.nowMs)){
    runLightingTask(context);
  }
  
  // Check for idle timeout and auto-return to dashboard
  if(IdleManager::update(context.nowMs)){
    // Idle timeout reached - return to home
    if(!AppNavigation::isAtHome()){
      Serial.println("[HOME RESET] Auto-return to dashboard triggered");
      LOG_INFO(LogTag::APP, "[IDLE] Auto-return to dashboard triggered");
      AppNavigation::goHome();
      AudioService::beep(150);  // Short beep for auto-return
    }
  }
}

void runUiTask(FrameContext &context){
  AppNavigation::commit();
  const uint32_t frameStartUs = micros();
  const unsigned long now = context.nowMs;
  DeviceSettings &settings = SettingsFlow::settings();

  uiTiming.decisionUs = 0;
  uiTiming.dataUs = 0;
  uiTiming.menuDrawingUs = 0;
  uiTiming.textRenderingUs = 0;
  uiTiming.iconRenderingUs = 0;
  uiTiming.animationRenderingUs = 0;
  uiTiming.displayRenderingUs = 0;
  uiTiming.spiTransferUs = 0;
  uiTiming.frameUs = 0;

  const uint32_t decisionStartUs = micros();
  const bool stateChanged = AppNavigation::hasStateChanged();
  const bool buildFrame = shouldBuildUiFrame(now, stateChanged);
  uiTiming.decisionUs = micros() - decisionStartUs;

  if(!buildFrame){
    uiTiming.framesSkipped++;
    uiTiming.frameUs = micros() - frameStartUs;
    return;
  }

  if(stateChanged){
    AppNavigation::clearStateChanged();
  }

  const uint32_t blinkStartUs = micros();
  (void)updateBlinkState(now);
  uiTiming.animationRenderingUs += micros() - blinkStartUs;

  const AppState currentState = AppNavigation::current();
  uiTiming.framesBuilt++;

  switch(currentState){
    case STATE_CLOCK: {
      const uint32_t animationStartUs = micros();
      ClockFeature::update(now,settings.quotes,settings.format24);
      uiTiming.animationRenderingUs += micros() - animationStartUs;

      const uint32_t renderStartUs = micros();
      UiScreens::renderClockScreen(ClockFeature::timeRow(),ClockFeature::quoteRow());
      uiTiming.textRenderingUs += micros() - renderStartUs;
      break;
    }

    case STATE_TIMER_VIEW:
    case STATE_TIMER_EDIT:
    case STATE_TIMER_ALARM: {
      const uint32_t dataStartUs = micros();
      UiScreens::TimerScreenData data = timerScreenData(now);
      uiTiming.dataUs += micros() - dataStartUs;

      const uint32_t renderStartUs = micros();
      UiScreens::renderTimerScreen(currentState,data,systemContext.ui.blinkState);
      uiTiming.textRenderingUs += micros() - renderStartUs;
      break;
    }

    case STATE_STOPWATCH: {
      const uint32_t animationStartUs = micros();
      StopwatchFeature::update(now);
      uiTiming.animationRenderingUs += micros() - animationStartUs;

      const uint32_t dataStartUs = micros();
      UiScreens::StopwatchScreenData data = stopwatchScreenData();
      uiTiming.dataUs += micros() - dataStartUs;

      const uint32_t renderStartUs = micros();
      UiScreens::renderStopwatchScreen(data);
      uiTiming.textRenderingUs += micros() - renderStartUs;
      break;
    }

    case STATE_REMINDER_HOME:
    case STATE_REMINDER_LIST:
    case STATE_REMINDER_EDIT: {
      const uint32_t dataStartUs = micros();
      UiScreens::RemindersScreenData data = remindersScreenData();
      uiTiming.dataUs += micros() - dataStartUs;

      const uint32_t renderStartUs = micros();
      UiScreens::renderRemindersScreen(currentState,data,settings.format24,systemContext.ui.blinkState);
      uiTiming.menuDrawingUs += micros() - renderStartUs;
      break;
    }

    case STATE_SETTINGS_HOME:
    case STATE_SETTINGS_MENU:
    case STATE_SETTINGS_EDIT: {
      const uint32_t dataStartUs = micros();
      SettingsFlow::Snapshot snapshot = SettingsFlow::snapshot(FIRMWARE_VERSION,systemContext.ui.blinkState);
      uiTiming.dataUs += micros() - dataStartUs;

      const uint32_t renderStartUs = micros();
      UiScreens::renderSettingsScreen(currentState,settings,snapshot);
      uiTiming.menuDrawingUs += micros() - renderStartUs;
      break;
    }

    case STATE_REMINDER_ALARM: {
      const uint32_t dataStartUs = micros();
      UiScreens::RemindersScreenData data = remindersScreenData();
      uiTiming.dataUs += micros() - dataStartUs;

      const uint32_t renderStartUs = micros();
      UiScreens::renderReminderAlarmScreen(data);
      uiTiming.textRenderingUs += micros() - renderStartUs;
      break;
    }
  }

  const uint32_t displayStartUs = micros();
  const bool flushed = flushUiFrame(false);
  uiTiming.displayRenderingUs = micros() - displayStartUs;
  if(!flushed){
    uiTiming.unnecessaryRedraws++;
  }
  systemContext.ui.lastRefreshMs=now;
  uiTiming.frameUs = micros() - frameStartUs;
  if(uiTiming.frameUs > uiTiming.maxFrameUs){
    uiTiming.maxFrameUs = uiTiming.frameUs;
  }
}

void runDiagnosticsTask(FrameContext &context){
  static uint32_t lastLedPerfMs = 0;
  static uint32_t lastLedPerfTx = 0;
  static uint32_t lastLedPerfAck = 0;
  static uint32_t lastLedPerfRetry = 0;
  static uint32_t lastLedPerfUartTxBytes = 0;

  const SchedulerStats &schedulerStats = scheduler.stats();
  const HardwareRequestStats &hardwareStats = HardwareRequests::stats();
  const EventQueueStats &eventStats = eventQueueStats();
  const FaultSnapshot &faults = FaultTracker::snapshot();
  const FaultRecord &lastFault = faults.lastFault;
  const Esp8266LinkDiagnostics &link = Esp8266Link::diagnostics();
  const UartMonitorStats &uart = UartTrafficMonitor::stats();
  const uint32_t ledPerfElapsedMs = lastLedPerfMs == 0 ? 0 : context.nowMs - lastLedPerfMs;
  const uint32_t ledTxDelta = link.ledTx - lastLedPerfTx;
  const uint32_t ledAckDelta = link.ledAck - lastLedPerfAck;
  const uint32_t ledRetryDelta = link.ledRetry - lastLedPerfRetry;
  const uint32_t uartTxByteDelta = uart.txBytes - lastLedPerfUartTxBytes;
  const uint32_t ledTxPerMinute =
    ledPerfElapsedMs == 0 ? 0 : (uint32_t)(((uint64_t)ledTxDelta * 60000ULL) / ledPerfElapsedMs);
  const uint32_t uartTxBytesPerMinute =
    ledPerfElapsedMs == 0 ? 0 : (uint32_t)(((uint64_t)uartTxByteDelta * 60000ULL) / ledPerfElapsedMs);

  lastLedPerfMs = context.nowMs;
  lastLedPerfTx = link.ledTx;
  lastLedPerfAck = link.ledAck;
  lastLedPerfRetry = link.ledRetry;
  lastLedPerfUartTxBytes = uart.txBytes;

  LOG_INFO(
    LogTag::APP,
    "sched loops=%lu tasks=%lu events q=%lu d=%lu drop=%u max=%u seq=%u hw q=%lu x=%lu drop=%u max=%u seq=%u age=%lums",
    (unsigned long)schedulerStats.loopCount,
    (unsigned long)schedulerStats.taskRunCount,
    (unsigned long)eventStats.queued,
    (unsigned long)eventStats.dequeued,
    eventStats.dropped,
    eventStats.maxDepth,
    eventStats.lastSequenceId,
    (unsigned long)hardwareStats.queued,
    (unsigned long)hardwareStats.executed,
    hardwareStats.dropped,
    hardwareStats.maxDepth,
    hardwareStats.lastSequenceId,
    (unsigned long)hardwareStats.maxCommandAgeMs
  );

  LOG_INFO(
    LogTag::APP,
    "OVERRUN count=%lu task=%s runtime=%luus budget=%luus excess=%luus at=%lums maxLoopActual=%luus",
    (unsigned long)schedulerStats.overrunCount,
    schedulerStats.lastOverrunTaskName != nullptr ? schedulerStats.lastOverrunTaskName : "none",
    (unsigned long)schedulerStats.lastOverrunRuntimeUs,
    (unsigned long)schedulerStats.lastOverrunBudgetUs,
    (unsigned long)schedulerStats.lastOverrunExcessUs,
    (unsigned long)schedulerStats.lastOverrunTimestampMs,
    (unsigned long)schedulerStats.maxLoopRuntimeUs
  );

  const LcdDriver::TimingStats &lcdTiming = LcdDriver::timingStats();
  LOG_INFO(
    LogTag::APP,
    "LCD_TIMING clear=%luus row1=%luus row2=%luus frame=%luus setCursor=%luus print=%luus write=%luus maxFrame=%luus ops clear=%lu frame=%lu cursor=%u print=%u write=%u chars=%u rows=%u skipped=%u runs=%u/%u",
    (unsigned long)lcdTiming.clearUs,
    (unsigned long)lcdTiming.row1Us,
    (unsigned long)lcdTiming.row2Us,
    (unsigned long)lcdTiming.frameUs,
    (unsigned long)lcdTiming.setCursorUs,
    (unsigned long)lcdTiming.printUs,
    (unsigned long)lcdTiming.writeUs,
    (unsigned long)lcdTiming.maxFrameUs,
    (unsigned long)lcdTiming.clearCount,
    (unsigned long)lcdTiming.frameUpdateCount,
    (unsigned)lcdTiming.lastFrameSetCursorOps,
    (unsigned)lcdTiming.lastFramePrintOps,
    (unsigned)lcdTiming.lastFrameWriteOps,
    (unsigned)lcdTiming.lastFrameChangedChars,
    (unsigned)lcdTiming.lastFrameRowsChanged,
    (unsigned)lcdTiming.lastFrameRowsSkipped,
    (unsigned)lcdTiming.lastRow1Runs,
    (unsigned)lcdTiming.lastRow2Runs
  );

  LOG_INFO(
    LogTag::APP,
    "UI_TIMING frame=%luus max=%luus decision=%luus data=%luus menu=%luus text=%luus icon=%luus animation=%luus display=%luus spi=%luus built=%lu flushed=%lu skipped=%lu unchanged=%lu full=%lu unnecessary=%lu",
    (unsigned long)uiTiming.frameUs,
    (unsigned long)uiTiming.maxFrameUs,
    (unsigned long)uiTiming.decisionUs,
    (unsigned long)uiTiming.dataUs,
    (unsigned long)uiTiming.menuDrawingUs,
    (unsigned long)uiTiming.textRenderingUs,
    (unsigned long)uiTiming.iconRenderingUs,
    (unsigned long)uiTiming.animationRenderingUs,
    (unsigned long)uiTiming.displayRenderingUs,
    (unsigned long)uiTiming.spiTransferUs,
    (unsigned long)uiTiming.framesBuilt,
    (unsigned long)uiTiming.framesFlushed,
    (unsigned long)uiTiming.framesSkipped,
    (unsigned long)uiTiming.unchangedFrames,
    (unsigned long)uiTiming.fullScreenRedraws,
    (unsigned long)uiTiming.unnecessaryRedraws
  );

  LOG_INFO(
    LogTag::APP,
    "LED_STATE desired=%s/%u/%u/%u active=%s/%u/%u/%u sent=%s/%u/%u/%u pending=%s/%u/%u/%u status=%s retry=%u ackAge=%lums err=%s counts tx=%lu ack=%lu retry=%lu dupIgnored=%lu",
    link.desiredLedMode,
    link.desiredLedPower ? 1 : 0,
    link.desiredLedBrightness,
    link.desiredLedSpeed,
    link.activeLedMode,
    link.activeLedPower ? 1 : 0,
    link.activeLedBrightness,
    link.activeLedSpeed,
    link.lastSentLedMode,
    link.lastSentLedPower ? 1 : 0,
    link.lastSentLedBrightness,
    link.lastSentLedSpeed,
    link.pendingLedMode,
    link.pendingLedPower ? 1 : 0,
    link.pendingLedBrightness,
    link.pendingLedSpeed,
    Esp8266Link::ledStatusName(link.ledStatus),
    link.ledRetryCount,
    link.lastLedAckMs == 0 ? 0UL : (unsigned long)(context.nowMs - link.lastLedAckMs),
    link.lastLedErrReason,
    (unsigned long)link.ledTx,
    (unsigned long)link.ledAck,
    (unsigned long)link.ledRetry,
    (unsigned long)link.ledDuplicateIgnored
  );

  LOG_INFO(
    LogTag::APP,
    "LED_PERF interval=%lums txDelta=%lu ackDelta=%lu retryDelta=%lu txRate=%lu/min uartTxBytesDelta=%lu uartTxBytesRate=%lu/min",
    (unsigned long)ledPerfElapsedMs,
    (unsigned long)ledTxDelta,
    (unsigned long)ledAckDelta,
    (unsigned long)ledRetryDelta,
    (unsigned long)ledTxPerMinute,
    (unsigned long)uartTxByteDelta,
    (unsigned long)uartTxBytesPerMinute
  );

  logTaskRuntimeProfiles();
  logOverrunAttributionSummary();

  LOG_INFO(
    LogTag::APP,
    "fault state=%s retry=%u recover=%u reset=%u syncReason=%s last=%s/%s repeat=%u reason=%s counters link=%lu proto=%lu sched=%lu hw=%lu drv=%lu uart tx=%lu/%luB rx=%lu/%luB crc=%lu to=%lu resync=%lu malformed=%lu",
    Esp8266Link::stateName(link.state),
    link.retryCount,
    link.recoveryAttemptCount,
    link.transportResetCount,
    link.lastSyncReason,
    FaultTracker::sourceName(lastFault.source),
    FaultTracker::codeName(lastFault.code),
    lastFault.repeatCount,
    link.lastRecoveryReason,
    (unsigned long)faults.counters.link,
    (unsigned long)faults.counters.protocol,
    (unsigned long)faults.counters.schedulerOverrun,
    (unsigned long)faults.counters.hardwareRequest,
    (unsigned long)faults.counters.driver,
    (unsigned long)uart.txPackets,
    (unsigned long)uart.txBytes,
    (unsigned long)uart.rxPackets,
    (unsigned long)uart.rxBytes,
    (unsigned long)uart.crcErrors,
    (unsigned long)uart.timeoutErrors,
    (unsigned long)uart.resyncCount,
    (unsigned long)uart.malformedPackets
  );
}

void logTaskRuntimeProfiles(){
  logTaskRuntimeRanking(0);
  logTaskRuntimeRanking(1);
  logTaskRuntimeRanking(2);
}

uint64_t taskMetricValue(const ScheduledTask &task, uint8_t metric){
  switch(metric){
    case 0:
      return task.maxRuntimeUs;
    case 1:
      return task.runCount == 0 ? 0 : task.totalRuntimeUs / task.runCount;
    case 2:
      return task.totalRuntimeUs;
  }
  return 0;
}

const char* taskMetricName(uint8_t metric){
  switch(metric){
    case 0:
      return "max";
    case 1:
      return "avg";
    case 2:
      return "total";
  }
  return "unknown";
}

void logTaskRuntimeRanking(uint8_t metric){
  constexpr uint8_t TopTaskLimit = 10;
  constexpr uint8_t MaxProfileTasks = 16;
  bool selected[MaxProfileTasks] = {};
  const uint8_t count = scheduler.taskCount();
  const uint8_t limit = count < TopTaskLimit ? count : TopTaskLimit;

  for(uint8_t rank = 0; rank < limit; ++rank){
    uint8_t bestIndex = 0;
    uint64_t bestValue = 0;
    bool found = false;

    for(uint8_t i = 0; i < count && i < MaxProfileTasks; ++i){
      if(selected[i]) continue;
      const ScheduledTask &task = scheduler.taskAt(i);
      const uint64_t value = taskMetricValue(task, metric);
      if(!found || value > bestValue){
        bestValue = value;
        bestIndex = i;
        found = true;
      }
    }

    if(!found) break;
    selected[bestIndex] = true;

    const ScheduledTask &task = scheduler.taskAt(bestIndex);
    const unsigned long avgRuntimeUs =
      task.runCount == 0 ? 0 : (unsigned long)(task.totalRuntimeUs / task.runCount);
    LOG_INFO(
      LogTag::APP,
      "[TASK_%s] rank=%u task=%s avg=%luus max=%luus last=%luus total=%lluus runs=%lu overruns=%lu budget=%luus overrunTotal=%lluus overrunExcess=%lluus",
      taskMetricName(metric),
      (unsigned)(rank + 1),
      task.name,
      (unsigned long)avgRuntimeUs,
      (unsigned long)task.maxRuntimeUs,
      (unsigned long)task.lastRuntimeUs,
      (unsigned long long)task.totalRuntimeUs,
      (unsigned long)task.runCount,
      (unsigned long)task.overrunCount,
      (unsigned long)task.budgetUs,
      (unsigned long long)task.totalOverrunRuntimeUs,
      (unsigned long long)task.totalOverrunExcessUs
    );
  }
}

void logOverrunAttributionSummary(){
  const SchedulerStats &schedulerStats = scheduler.stats();
  if(schedulerStats.overrunCount == 0){
    LOG_INFO(LogTag::APP, "OVERRUN_ROOT_CAUSE none count=0");
    return;
  }

  const ScheduledTask *leader = nullptr;
  for(uint8_t i = 0; i < scheduler.taskCount(); ++i){
    const ScheduledTask &task = scheduler.taskAt(i);
    if(leader == nullptr || task.overrunCount > leader->overrunCount){
      leader = &task;
    }
  }

  if(leader == nullptr){
    LOG_INFO(LogTag::APP, "OVERRUN_ROOT_CAUSE unknown count=%lu", (unsigned long)schedulerStats.overrunCount);
    return;
  }

  const uint32_t countSharePct =
    (uint32_t)(((uint64_t)leader->overrunCount * 100ULL) / schedulerStats.overrunCount);
  LOG_INFO(
    LogTag::APP,
    "OVERRUN_ROOT_CAUSE task=%s count=%lu share=%lu%% budget=%luus max=%luus avg=%luus overrunTotal=%lluus overrunExcess=%lluus",
    leader->name,
    (unsigned long)leader->overrunCount,
    (unsigned long)countSharePct,
    (unsigned long)leader->budgetUs,
    (unsigned long)leader->maxRuntimeUs,
    (unsigned long)(leader->runCount == 0 ? 0 : leader->totalRuntimeUs / leader->runCount),
    (unsigned long long)leader->totalOverrunRuntimeUs,
    (unsigned long long)leader->totalOverrunExcessUs
  );
}

}

namespace Application {

void setup(){
  initHardware();
  TimerFeature::begin();

  while(!bootAnimation(millis())){
    flushUiFrame();
    HardwareRequests::executePending();
    AudioService::service();
  }
  flushUiFrame();

  const unsigned long now = millis();
  ClockFeature::begin(now);
  HardwareRequests::clearDisplay();
  UiScreens::clearFrame();
  lastUiFrameValid = false;
  scheduler.reset(now);
  LOG_INFO(LogTag::APP, "DeskDroid %s ready with %u scheduled tasks", FIRMWARE_VERSION, scheduler.taskCount());
}

void loop(){
  scheduler.run(millis());
  delay(1);
}

}