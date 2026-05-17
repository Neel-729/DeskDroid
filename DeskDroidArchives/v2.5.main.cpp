#include <esp_system.h>
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <pgmspace.h>
#include <Preferences.h>
#include <string.h>
#include <Adafruit_NeoPixel.h>

#define FirmwareVersion "2.5"

#define LED_PIN 13
#define LED_COUNT 83

constexpr uint8_t ENCODER_CLK = 19;
constexpr uint8_t ENCODER_DT  = 18;
constexpr uint8_t ENCODER_SW  = 5;
constexpr uint8_t BUZZER_PIN  = 4;

Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
LiquidCrystal_I2C lcd(0x27,16,2);
RTC_DS1307 rtc;
Preferences prefs;

// ================= UI STATE =================

enum AppState {
  STATE_CLOCK,
  STATE_TIMER_VIEW,
  STATE_TIMER_EDIT,
  STATE_TIMER_ALARM,
  STATE_STOPWATCH,
  STATE_REMINDER_HOME,
  STATE_REMINDER_LIST,
  STATE_REMINDER_EDIT,
  STATE_REMINDER_ALARM,
  STATE_SETTINGS_HOME,
  STATE_SETTINGS_MENU,
  STATE_SETTINGS_EDIT
};

AppState currentState = STATE_CLOCK;
AppState resumeStateAfterReminder = STATE_CLOCK;
bool stateChanged = true;

// ================= INPUT EVENTS =================

enum InputEvent {
  EVENT_NONE,
  EVENT_CLICK,
  EVENT_DOUBLE_CLICK,
  EVENT_LONG_PRESS,
  EVENT_ROTATE_CW,
  EVENT_ROTATE_CCW
};

// ================= NeoPixel =================

enum LedState {
  LED_IDLE,
  LED_TIMER_ALARM,
  LED_REMINDER_ALARM,
  LED_SUCCESS
};

LedState currentLedState = LED_IDLE;

enum LedIdlePreset {
  IDLE_OFF,
  IDLE_STATIC,
  IDLE_BREATH,
  IDLE_RAINBOW,
  IDLE_PULSE
};

LedIdlePreset idlePreset = IDLE_STATIC;


// ================= TIMERS =================

unsigned long lastDisplayRefresh = 0;
unsigned long lastQuoteScroll = 0;
unsigned long quotePauseUntil = 0;
unsigned long lastRTCUpdate = 0;
unsigned long beepUntil = 0;
unsigned long lastLightScheduleCheck = 0;
bool lightScheduleAllowsOutput = true;

constexpr uint16_t LONG_PRESS = 600;
constexpr uint16_t DOUBLE_PRESS_TIME = 300;

// ================= BUTTON ENGINE =================

bool buttonDown=false;
unsigned long buttonDownTime=0;

bool singlePressWaiting=false;
unsigned long singlePressTime=0;

// ================= RTC CACHE =================

DateTime cachedTime;

// ================= TIMER =================

int timerHours=0;
int timerMinutes=5;
int timerSeconds=0;

unsigned long timerEndTime=0;
unsigned long timerRemainingMillis=0;
unsigned long timerTotalMillis=0;

bool timerRunning=false;

unsigned long lastAlarmBeep=0;
unsigned long alarmStartTime=0;

// ================= TIMER EDIT =================

enum TimerEditField { EDIT_HOURS, EDIT_MINUTES, EDIT_SECONDS };
TimerEditField timerEditField=EDIT_MINUTES;

// ================= BLINK =================

bool blinkState=true;
unsigned long lastBlink=0;

// ================= STOPWATCH =================

unsigned long stopwatchStartTime=0;
unsigned long stopwatchElapsed=0;
bool stopwatchRunning=false;

// ================= QUOTES =================

const char* const quotes[] PROGMEM={
"Discipline is freedom.",
"Focus beats talent.",
"Action creates clarity.",
"Do the hard things first.",
"Consistency beats intensity.",
"Small steps daily.",
"Build systems not goals.",
"Clarity follows action.",
"Momentum creates motivation.",
"Effort compounds."
};

const uint8_t QUOTE_COUNT = sizeof(quotes)/sizeof(quotes[0]);

uint8_t lastQuote=255;

// Bitmask to track used quotes (supports up to 32 quotes)
uint32_t quoteUsedMask = 0;
uint8_t quoteUsedCount = 0;

uint8_t pickQuote(){
  if(quoteUsedCount >= QUOTE_COUNT){
    quoteUsedMask = 0;
    quoteUsedCount = 0;
  }

  uint8_t q;
  do{
    q = random(QUOTE_COUNT);
  }while( (quoteUsedMask & (1UL << q)) || q == lastQuote );

  quoteUsedMask |= (1UL << q);
  quoteUsedCount++;
  lastQuote = q;

  return q;
}

char activeQuote[96];
int currentQuoteIdx=0;
int scrollPosition=0;
int activeQuoteLength=0;

char timeRow[17];
char quoteRow[17];

const char* days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

// ================= SETTINGS =================

struct Settings {
  bool buzzer=true;
  bool quotes=true;
  bool format24=true;
  uint8_t brightness=4;
  uint8_t ledBrightness=5;
  uint8_t idlePreset = 1;
  bool autoLights = true;
  uint8_t lightsOnHour = 7;
  uint8_t lightsOnMinute = 0;
  uint8_t lightsOffHour = 22;
  uint8_t lightsOffMinute = 0;
};

Settings deviceSettings;

const char* settingsMenu[]={
  "Backlight",
  "LED Mode",
  "LED Brightness",
  "Auto Lights",
  "Lights On",
  "Lights Off",
  "Buzzer",
  "Quotes",
  "Time Format",
  "Adjust Time",
  "Adjust Date",
  "About"
};

const uint8_t SETTINGS_COUNT=12;
uint8_t settingsIndex=0;
uint8_t adjustHour=0;
uint8_t adjustMinute=0;
bool adjustHourField=true;
unsigned long rtcSyncMessageUntil=0;

enum ScheduleEditField { SCHEDULE_HOUR, SCHEDULE_MINUTE };
ScheduleEditField scheduleEditField = SCHEDULE_HOUR;

// --- Date adjust variables (new) ---
uint16_t adjustYear=2025;
uint8_t adjustMonth=1;
uint8_t adjustDay=1;

enum DateField { DATE_DAY, DATE_MONTH, DATE_YEAR };
DateField adjustDateField = DATE_DAY;

// ================= ENCODER =================

int8_t readEncoder(){
  static uint8_t prevState=0;
  static int8_t stepAccum=0;

  prevState<<=2;

  if(digitalRead(ENCODER_CLK)) prevState|=0x02;
  if(digitalRead(ENCODER_DT))  prevState|=0x01;

  prevState&=0x0F;

  if(prevState==0b1101||prevState==0b0100||prevState==0b0010||prevState==0b1011) stepAccum++;
  if(prevState==0b1110||prevState==0b0111||prevState==0b0001||prevState==0b1000) stepAccum--;

  if(stepAccum>=2){ stepAccum=0; return 1; }
  if(stepAccum<=-2){ stepAccum=0; return -1; }

  return 0;
}

// ================= BUZZER =================

void triggerBeep(int duration=50){
  digitalWrite(BUZZER_PIN,HIGH);
  beepUntil=millis()+duration;
}

void updateBuzzer(){
  if(digitalRead(BUZZER_PIN)==HIGH && millis()>beepUntil)
    digitalWrite(BUZZER_PIN,LOW);
}

// ================= LCD BUFFER =================

char lcdBuffer[2][17]={{0}};

void writeRow(int row,const char* s){
  char buf[17];
  memset(buf,' ',16);
  buf[16]='\0';
  for(int i=0;i<16 && s[i]!='\0';i++) buf[i]=s[i];

  if(strncmp(lcdBuffer[row],buf,16)==0) return;
  strcpy(lcdBuffer[row],buf);
  lcd.setCursor(0,row);
  lcd.print(buf);
}

// ================= STATE HELPERS =================

const AppState MAIN_STATES[] = {
  STATE_CLOCK,
  STATE_TIMER_VIEW,
  STATE_STOPWATCH,
  STATE_REMINDER_HOME,
  STATE_SETTINGS_HOME
};

const uint8_t MAIN_STATE_COUNT = sizeof(MAIN_STATES) / sizeof(MAIN_STATES[0]);

void enterState(AppState nextState){
  if(currentState != nextState){
    currentState = nextState;
  }
  stateChanged = true;
}

bool isSettingsState(){
  return currentState==STATE_SETTINGS_HOME ||
         currentState==STATE_SETTINGS_MENU ||
         currentState==STATE_SETTINGS_EDIT;
}

bool isTimerState(){
  return currentState==STATE_TIMER_VIEW ||
         currentState==STATE_TIMER_EDIT ||
         currentState==STATE_TIMER_ALARM;
}

bool isReminderState(){
  return currentState==STATE_REMINDER_HOME ||
         currentState==STATE_REMINDER_LIST ||
         currentState==STATE_REMINDER_EDIT ||
         currentState==STATE_REMINDER_ALARM;
}

int mainStateIndex(AppState state){
  for(uint8_t i=0;i<MAIN_STATE_COUNT;i++){
    if(MAIN_STATES[i] == state) return i;
  }
  return 0;
}

void rotateMainState(int step){
  int idx = mainStateIndex(currentState) + step;
  if(idx < 0) idx = MAIN_STATE_COUNT - 1;
  if(idx >= MAIN_STATE_COUNT) idx = 0;
  enterState(MAIN_STATES[idx]);
}

// ================= LIGHT SCHEDULE =================

uint16_t minutesSinceMidnight(uint8_t hour, uint8_t minute){
  return (uint16_t)hour * 60 + minute;
}

bool isWithinLightSchedule(const DateTime &now){
  uint16_t current = minutesSinceMidnight(now.hour(), now.minute());
  uint16_t start = minutesSinceMidnight(deviceSettings.lightsOnHour, deviceSettings.lightsOnMinute);
  uint16_t end = minutesSinceMidnight(deviceSettings.lightsOffHour, deviceSettings.lightsOffMinute);

  if(start == end) return true;
  if(start < end) return current >= start && current < end;
  return current >= start || current < end;
}

bool lightsAllowedNow(){
  return lightScheduleAllowsOutput;
}

void updateLightScheduleState(){
  lightScheduleAllowsOutput = !deviceSettings.autoLights || isWithinLightSchedule(rtc.now());
}

void applyBacklightSetting(){
  if(deviceSettings.brightness && lightScheduleAllowsOutput) lcd.backlight();
  else lcd.noBacklight();
}

void refreshLightOutputs(){
  updateLightScheduleState();
  applyBacklightSetting();
}

// ================= BOOT =================

bool bootAnimation(){
  static uint8_t stage=0;
  static unsigned long lastStep=0;

  const char* frames[]={
    "Booting","Booting.","Booting..","Booting...","Booting....","Booting...","Booting....","Booting....."
  };

  if(stage==0){
    writeRow(0,"DeskDroid v" FirmwareVersion);
    triggerBeep();
    lastStep=millis();
    stage=1;
  }

  if(stage>=1 && stage<=8){
    if(millis()-lastStep>150){
      writeRow(1,frames[stage-1]);
      lastStep=millis();
      if(stage==6) triggerBeep(50);
      if(stage==8) triggerBeep(50);
      stage++;
    }
  }

  if(stage>8){
    stage=0;
    return true;
  }
  return false;
}

// ================= QUOTES =================

void loadQuote(){
  const char* ptr=(const char*)pgm_read_ptr(&(quotes[currentQuoteIdx]));
  strncpy_P(activeQuote,ptr,sizeof(activeQuote)-1);
  activeQuote[sizeof(activeQuote)-1]='\0';
  activeQuoteLength=strlen(activeQuote);

  int pad=16;
  if(activeQuoteLength+pad>=sizeof(activeQuote)-1) pad=sizeof(activeQuote)-1-activeQuoteLength;
  for(int i=0;i<pad;i++) activeQuote[activeQuoteLength+i]=' ';
  activeQuoteLength+=pad;
  activeQuote[activeQuoteLength]='\0';

  scrollPosition=0;
  for(int i=0;i<16;i++) quoteRow[i]=activeQuote[i];
  quoteRow[16]='\0';
  quotePauseUntil=millis()+2500;
}

void updateQuoteScroll(){
  if(!deviceSettings.quotes){
    writeRow(1,"                ");
    return;
  }
  if(millis()<quotePauseUntil) return;
  if(millis()-lastQuoteScroll>350){
    for(int i=0;i<16;i++)
      quoteRow[i]=activeQuote[(scrollPosition+i)%activeQuoteLength];
    quoteRow[16]='\0';
    scrollPosition++;
    if(scrollPosition>=activeQuoteLength){
      currentQuoteIdx=pickQuote();
      loadQuote();
    }
    lastQuoteScroll=millis();
  }
}

// ================= CLOCK =================

void updateClockTime(){
  if(millis()-lastRTCUpdate>1000){
    cachedTime=rtc.now();
    lastRTCUpdate=millis();
  }

  int hour=cachedTime.hour();
  if(!deviceSettings.format24){
    if(hour==0) hour=12;
    else if(hour>12) hour-=12;
  }

  snprintf(timeRow,sizeof(timeRow),"%02d:%02d|%s %s %02d",
    hour,
    cachedTime.minute(),
    days[cachedTime.dayOfTheWeek()],
    months[cachedTime.month()-1],
    cachedTime.day()
    );
}

void renderClockScreen(){
  updateClockTime();
  updateQuoteScroll();
  writeRow(0,timeRow);
  writeRow(1,quoteRow);
}

// ================= SETTINGS =================

void saveSettings(){
  prefs.putBool("buzzer",deviceSettings.buzzer);
  prefs.putBool("quotes",deviceSettings.quotes);
  prefs.putBool("24h",deviceSettings.format24);
  prefs.putUChar("bright",deviceSettings.brightness);
  prefs.putUChar("ledb", deviceSettings.ledBrightness);
  prefs.putUChar("ledmode", deviceSettings.idlePreset);
  prefs.putBool("autol", deviceSettings.autoLights);
  prefs.putUChar("lonh", deviceSettings.lightsOnHour);
  prefs.putUChar("lonm", deviceSettings.lightsOnMinute);
  prefs.putUChar("loffh", deviceSettings.lightsOffHour);
  prefs.putUChar("loffm", deviceSettings.lightsOffMinute);
}

void resetSettingsEditModes(){
  adjustHourField=true;
  adjustDateField=DATE_DAY;
  scheduleEditField=SCHEDULE_HOUR;
}

void leaveSettingsMenu(){
  resetSettingsEditModes();
  enterState(STATE_SETTINGS_HOME);
}

void exitSettingsToClock(){
  resetSettingsEditModes();
  enterState(STATE_CLOCK);
}

void renderLightScheduleEdit(const char* title, uint8_t hour, uint8_t minute){
  writeRow(0,title);
  if(millis()-lastBlink>500){ blinkState=!blinkState; lastBlink=millis(); }

  int h=hour;
  int m=minute;
  if(currentState==STATE_SETTINGS_EDIT && !blinkState){
    if(scheduleEditField==SCHEDULE_HOUR) h=-1;
    else m=-1;
  }

  char hbuf[3];
  char mbuf[3];
  if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,3,"%02d",h);
  if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,3,"%02d",m);

  char buf[17];
  snprintf(buf,sizeof(buf),"%s:%s",hbuf,mbuf);
  writeRow(1,buf);
}

void renderSettingsScreen(){
  if(rtcSyncMessageUntil && millis()<rtcSyncMessageUntil){
    writeRow(0,"RTC Synced");
    writeRow(1,"Done");
    return;
  }

  if(currentState==STATE_SETTINGS_HOME){
    writeRow(0,"Settings");
    writeRow(1,"Enter");
  }
  else if(currentState==STATE_SETTINGS_MENU){
    writeRow(0,"Settings");
    char buf[17];
    snprintf(buf,sizeof(buf),"> %s",settingsMenu[settingsIndex]);
    writeRow(1,buf);
  }
  else if(currentState==STATE_SETTINGS_EDIT){
    switch(settingsIndex){
      case 0: writeRow(0,"Backlight"); writeRow(1,deviceSettings.brightness?"ON":"OFF"); break;
      
      case 1:
      writeRow(0,"LED Mode");
      {
        const char* modes[] = {"Off","Static","Breath","Rainbow","Pulse"};
        writeRow(1, modes[deviceSettings.idlePreset]);
      }
       break;
      
      case 2:
      writeRow(0,"LED Brightness");
     {
      char buf[17];
      snprintf(buf,sizeof(buf),"Level: %d",deviceSettings.ledBrightness);
      writeRow(1,buf);
      }
      break;
      case 3: writeRow(0,"Auto Lights"); writeRow(1,deviceSettings.autoLights?"ON":"OFF"); break;
      case 4:
        renderLightScheduleEdit("Lights On", deviceSettings.lightsOnHour, deviceSettings.lightsOnMinute);
        break;
      case 5:
        renderLightScheduleEdit("Lights Off", deviceSettings.lightsOffHour, deviceSettings.lightsOffMinute);
        break;
      case 6: writeRow(0,"Buzzer"); writeRow(1,deviceSettings.buzzer?"ON":"OFF"); break;
      case 7: writeRow(0,"Quotes"); writeRow(1,deviceSettings.quotes?"ON":"OFF"); break;
      case 8: writeRow(0,"Time Format"); writeRow(1,deviceSettings.format24?"24H":"12H"); break;
      case 9:{
        writeRow(0,"Adjust Time");
        if(millis()-lastBlink>500){ blinkState=!blinkState; lastBlink=millis(); }
        int h=adjustHour; int m=adjustMinute;
        if(!blinkState){
          if(adjustHourField) h=-1; else m=-1;
        }
        char hbuf[3]; char mbuf[3];
        if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,3,"%02d",h);
        if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,3,"%02d",m);
        char buf[17]; snprintf(buf,sizeof(buf),"%s:%s",hbuf,mbuf);
        writeRow(1,buf);
        break;
      }
      case 10:{
        // Adjust Date UI (new)
        writeRow(0,"Adjust Date");
        if(millis()-lastBlink>500){ blinkState=!blinkState; lastBlink=millis(); }

        int d=adjustDay;
        int mo=adjustMonth;
        int y=adjustYear;

        if(!blinkState){
          if(adjustDateField==DATE_DAY) d=-1;
          else if(adjustDateField==DATE_MONTH) mo=-1;
          else y=-1;
        }

        char dbuf[3]; char mbuf[3]; char ybuf[5];
        if(d<0) strcpy(dbuf,"  "); else snprintf(dbuf,3,"%02d",d);
        if(mo<0) strcpy(mbuf,"  "); else snprintf(mbuf,3,"%02d",mo);
        if(y<0) strcpy(ybuf,"    "); else snprintf(ybuf,5,"%04d",y);

        char buf[17];
        snprintf(buf,sizeof(buf),"%s/%s/%s",dbuf,mbuf,ybuf);
        writeRow(1,buf);
        break;
      }
      case 11: writeRow(0,"DeskDroid"); writeRow(1,"v" FirmwareVersion); break;
    }
  }
}

// ================= TIMER =================

void renderTimerScreen(){
  if(currentState==STATE_TIMER_ALARM){
    writeRow(0,"Timer Finished!");
    unsigned long total=timerTotalMillis;
    int h=total/3600000;
    int m=(total/60000)%60;
    int s=(total/1000)%60;
    char buf[17];
    snprintf(buf,sizeof(buf),"%02d:%02d:%02d",h,m,s);
    writeRow(1,buf);
    return;
  }

  if(currentState==STATE_TIMER_EDIT){
    writeRow(0,"Timer > Edit");
    if(millis()-lastBlink>500){ blinkState=!blinkState; lastBlink=millis(); }
    int h=timerHours; int m=timerMinutes; int s=timerSeconds;
    if(!blinkState){
      if(timerEditField==EDIT_HOURS) h=-1;
      if(timerEditField==EDIT_MINUTES) m=-1;
      if(timerEditField==EDIT_SECONDS) s=-1;
    }
    char hbuf[3], mbuf[3], sbuf[3];
    if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,3,"%02d",h);
    if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,3,"%02d",m);
    if(s<0) strcpy(sbuf,"  "); else snprintf(sbuf,3,"%02d",s);
    char buf[17]; snprintf(buf,sizeof(buf),"%s:%s:%s",hbuf,mbuf,sbuf);
    writeRow(1,buf);
    return;
  }

  if(timerRunning){
    if(millis()-lastBlink>500){ blinkState=!blinkState; lastBlink=millis(); }
    if(blinkState) writeRow(0,"Timer Running"); else writeRow(0,"Timer        ");
  } else {
    if(timerRemainingMillis==timerTotalMillis) writeRow(0,"Timer"); else writeRow(0,"Timer Paused");
  }
}

void updateTimer(){
  if(currentState==STATE_TIMER_ALARM) return;
  unsigned long remaining;
  if(timerRunning){
    if(millis()>=timerEndTime){
      timerRunning=false;
      timerRemainingMillis=0;
      enterState(STATE_TIMER_ALARM);
      currentLedState = LED_TIMER_ALARM;
      alarmStartTime=millis();
      return;
    }
    remaining=timerEndTime-millis();
  } else {
    remaining=timerRemainingMillis;
  }
  if(currentState!=STATE_TIMER_EDIT){
    int h=remaining/3600000;
    int m=(remaining/60000)%60;
    int s=(remaining/1000)%60;
    char buf[17];
    snprintf(buf,sizeof(buf),"%02d:%02d:%02d",h,m,s);
    writeRow(1,buf);
  }
}

// ================= STOPWATCH =================

void renderStopwatchScreen(){
  writeRow(0,"Stopwatch");
  if(!stopwatchRunning && stopwatchElapsed==0) writeRow(1,"00:00.00");
  else if(!stopwatchRunning) writeRow(1,"Paused");
}

void updateStopwatch(){
  if(stopwatchRunning) stopwatchElapsed = (unsigned long)(millis() - stopwatchStartTime);
  unsigned long t=stopwatchElapsed;
  int minutes=(t/60000)%60;
  int seconds=(t/1000)%60;
  int ms=(t%1000)/10;
  char buf[17];
  snprintf(buf,sizeof(buf),"%02d:%02d.%02d",minutes,seconds,ms);
  writeRow(1,buf);
}

// ================= REMINDERS (UI + Engine) =================

struct Reminder { uint8_t hour; uint8_t minute; bool active; };
constexpr uint8_t MAX_REMINDERS = 5;
Reminder reminders[MAX_REMINDERS];

uint8_t reminderIndex = 0;

// field selection
enum ReminderEditField { REM_EDIT_HOUR, REM_EDIT_MINUTE };
ReminderEditField reminderField = REM_EDIT_HOUR;

// alarm engine
uint8_t activeReminder = 255;
unsigned long reminderAlarmStart = 0;
unsigned long lastReminderBeep = 0;
uint8_t reminderBeepStage = 0;
uint32_t lastReminderTriggerStamp = 0xFFFFFFFF;

void saveReminders(){
  for(uint8_t i=0;i<MAX_REMINDERS;i++){
    char key[6];
    snprintf(key,sizeof(key),"r%dh",i);
    prefs.putUChar(key, reminders[i].hour);
    snprintf(key,sizeof(key),"r%dm",i);
    prefs.putUChar(key, reminders[i].minute);
    snprintf(key,sizeof(key),"r%da",i);
    prefs.putBool(key, reminders[i].active);
  }
}

void loadReminders(){
  for(uint8_t i=0;i<MAX_REMINDERS;i++){
    char key[6];
    snprintf(key,sizeof(key),"r%dh",i);
    reminders[i].hour = prefs.getUChar(key, 8); // default 08:00
    snprintf(key,sizeof(key),"r%dm",i);
    reminders[i].minute = prefs.getUChar(key, 0);
    snprintf(key,sizeof(key),"r%da",i);
    reminders[i].active = prefs.getBool(key, false);
  }
}

void startReminderAlarm(uint8_t idx){
  resumeStateAfterReminder = currentState;
  currentLedState = LED_REMINDER_ALARM;
  activeReminder = idx;
  reminderAlarmStart = millis();
  reminderBeepStage = 0;
  lastReminderBeep = 0;
  enterState(STATE_REMINDER_ALARM);
}

void stopReminderAlarm(){
  currentLedState = LED_IDLE;
  enterState(resumeStateAfterReminder);
}

void checkReminders(){
  DateTime now = rtc.now();
  uint32_t minuteStamp = (uint32_t)now.day() * 1440UL + (uint32_t)now.hour() * 60UL + now.minute();
  if(minuteStamp == lastReminderTriggerStamp) return;
  for(uint8_t i=0;i<MAX_REMINDERS;i++){
    if(!reminders[i].active) continue;
    if(now.hour()==reminders[i].hour && now.minute()==reminders[i].minute){
      lastReminderTriggerStamp = minuteStamp;
      startReminderAlarm(i);
      break;
    }
  }
}

// alarm pattern: 3 short beeps, pause, repeat up to 1 minute or until button
void updateReminderAlarm(){
  if(currentState!=STATE_REMINDER_ALARM) return;
  unsigned long now = millis();
  if(now - reminderAlarmStart > 60000){
    stopReminderAlarm();
    return;
  }

  // produce grouped pattern: 3 short beeps (80ms) separated by 120ms, then 400ms pause
  if(now - lastReminderBeep > 120){
    // beep on stages 0,2,4 (we'll map)
    uint8_t stage = reminderBeepStage % 8;
    if(stage==0 || stage==2 || stage==4){
      triggerBeep(80);
    }
    reminderBeepStage++;
    if(reminderBeepStage >= 8){
      reminderBeepStage = 0;
      lastReminderBeep = now + 400; // extra pause
    } else {
      lastReminderBeep = now;
    }
  }

  // display alarm screen
  char buf0[17];
  snprintf(buf0,sizeof(buf0),"Reminder %d",activeReminder+1);
  char buf1[17];
  snprintf(buf1,sizeof(buf1),"%02d:%02d", reminders[activeReminder].hour, reminders[activeReminder].minute);
  writeRow(0,buf0);
  writeRow(1,buf1);
}

bool getNextReminder(uint8_t &h, uint8_t &m){
  DateTime now = rtc.now();

  int nowMinutes = now.hour()*60 + now.minute();
  int bestDiff = 1440; // max minutes in a day
  bool found = false;

  for(uint8_t i=0;i<MAX_REMINDERS;i++){
    if(!reminders[i].active) continue;

    int remMinutes = reminders[i].hour*60 + reminders[i].minute;
    int diff = remMinutes - nowMinutes;

    if(diff < 0) diff += 1440; // wrap to tomorrow

    if(diff < bestDiff){
      bestDiff = diff;
      h = reminders[i].hour;
      m = reminders[i].minute;
      found = true;
    }
  }

  return found;
}

void renderRemindersScreen(){
  if(currentState == STATE_REMINDER_HOME){
    writeRow(0,"Reminders");

  uint8_t h,m;
  if(getNextReminder(h,m)){
    char buf[17];

    int displayHour = h;

    if(!deviceSettings.format24){
    if(displayHour == 0) displayHour = 12;
    else if(displayHour > 12) displayHour -= 12;
    }
    snprintf(buf,sizeof(buf),"Next at %02d:%02d",displayHour,m);
    writeRow(1,buf);
  }else{
    writeRow(1,"No reminders");
  }

  return;

  }

  if(currentState == STATE_REMINDER_LIST){
    char top[17];
    snprintf(top,sizeof(top),"Rem %d/%d %s", reminderIndex+1, MAX_REMINDERS, reminders[reminderIndex].active ? "ON ":"OFF");
    writeRow(0, top);
    if(!reminders[reminderIndex].active){
      writeRow(1,"Empty");
      return;
    }
    char buf[17];
    snprintf(buf,sizeof(buf),"%02d:%02d", reminders[reminderIndex].hour, reminders[reminderIndex].minute);
    writeRow(1, buf);
    return;
  }

  if(currentState == STATE_REMINDER_EDIT){
    writeRow(0,"Edit Reminder");
    if(millis()-lastBlink>500){ blinkState=!blinkState; lastBlink=millis(); }
    int rawHour = reminders[reminderIndex].hour;
    int h = rawHour;
    int m = reminders[reminderIndex].minute;

    if(!deviceSettings.format24){
     if(h == 0) h = 12;
     else if(h > 12) h -= 12;
}
    if(!blinkState){
      if(reminderField==REM_EDIT_HOUR) h=-1; else m=-1;
    }
    char hbuf[3]; char mbuf[3];
    if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,3,"%02d",h);
    if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,3,"%02d",m);
    char buf[17];
    snprintf(buf,sizeof(buf),"%s:%s %s", hbuf, mbuf, reminders[reminderIndex].active ? "ON":"OFF");
    writeRow(1, buf);
    return;
  }
}

// ================= HARDWARE INIT =================

void initHardware(){
  Serial.begin(115200);
  Wire.begin(21,22);
  lcd.init();
  lcd.backlight();
  uint8_t block[8]={255,255,255,255,255,255,255,255};
  lcd.createChar(0,block);

  if(!rtc.begin()){
    writeRow(0,"RTC ERROR");
    while(true);
  }
  if(!rtc.isrunning()){
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(ENCODER_CLK,INPUT_PULLUP);
  pinMode(ENCODER_DT,INPUT_PULLUP);
  pinMode(ENCODER_SW,INPUT_PULLUP);
  pinMode(BUZZER_PIN,OUTPUT);

  prefs.begin("desk", false);
  deviceSettings.buzzer = prefs.getBool("buzzer", true);
  deviceSettings.quotes = prefs.getBool("quotes", true);
  deviceSettings.format24 = prefs.getBool("24h", true);
  deviceSettings.brightness = prefs.getUChar("bright", 1);
  deviceSettings.autoLights = prefs.getBool("autol", true);
  deviceSettings.lightsOnHour = prefs.getUChar("lonh", 7);
  deviceSettings.lightsOnMinute = prefs.getUChar("lonm", 0);
  deviceSettings.lightsOffHour = prefs.getUChar("loffh", 22);
  deviceSettings.lightsOffMinute = prefs.getUChar("loffm", 0);
  if(deviceSettings.lightsOnHour > 23) deviceSettings.lightsOnHour = 7;
  if(deviceSettings.lightsOnMinute > 59) deviceSettings.lightsOnMinute = 0;
  if(deviceSettings.lightsOffHour > 23) deviceSettings.lightsOffHour = 22;
  if(deviceSettings.lightsOffMinute > 59) deviceSettings.lightsOffMinute = 0;
  updateLightScheduleState();
  applyBacklightSetting();

  loadReminders();
  randomSeed(esp_random());

  deviceSettings.ledBrightness = prefs.getUChar("ledb", 6);
  if(deviceSettings.ledBrightness > 10) deviceSettings.ledBrightness = 6;
  pixels.begin();
  pixels.setBrightness(deviceSettings.ledBrightness * 25.5); // scale 0-10 to 0-255
  pixels.clear();
  deviceSettings.idlePreset = prefs.getUChar("ledmode", 1);
  if(deviceSettings.idlePreset > IDLE_PULSE) deviceSettings.idlePreset = IDLE_STATIC;
  idlePreset = (LedIdlePreset)deviceSettings.idlePreset;
  pixels.show();

}

// ================= INPUT ENGINE =================

InputEvent readInput(){
  int movement=readEncoder();
  if(movement>0) return EVENT_ROTATE_CW;
  if(movement<0) return EVENT_ROTATE_CCW;

  bool pressed = digitalRead(ENCODER_SW)==LOW;
  unsigned long now = millis();

  if(pressed && !buttonDown){
    buttonDown=true;
    buttonDownTime=now;
  }

  if(!pressed && buttonDown){
    unsigned long duration=now-buttonDownTime;
    buttonDown=false;
    if(duration>LONG_PRESS) return EVENT_LONG_PRESS;

    if(singlePressWaiting && (now-singlePressTime<DOUBLE_PRESS_TIME)){
      singlePressWaiting=false;
      return EVENT_DOUBLE_CLICK;
    }
    singlePressWaiting=true;
    singlePressTime=now;
  }

  if(singlePressWaiting && (now-singlePressTime>DOUBLE_PRESS_TIME)){
    singlePressWaiting=false;
    return EVENT_CLICK;
  }

  return EVENT_NONE;
}


// ================= LED UPDATE =================

void updateLEDs(){
  static unsigned long lastUpdate = 0;
  static bool toggle = false;

  if(millis() - lastUpdate < 30) return;
  lastUpdate = millis();
  toggle = !toggle;

  switch(currentLedState){

case LED_IDLE:

  if(!lightsAllowedNow()){
    pixels.clear();
    break;
  }

  switch(idlePreset){

    case IDLE_OFF:
      pixels.clear();
      break;

    case IDLE_STATIC:
      pixels.fill(pixels.Color(30,0,20));
      break;

    case IDLE_BREATH: {
      static float phase = 0.0;
      static uint16_t baseHue = 0;

      phase += 0.06;  // breathing speed

      // Wrap phase cleanly
      if(phase > TWO_PI){
        phase -= TWO_PI;
        baseHue += 1800;  
      }

      // Smooth breathing wave
      float wave = (sin(phase) + 1.0) * 0.5;

      // set minimum brightness (0.1 = 10%, tweak this)
      float minLevel = 0.15;

      // scale wave to never reach 0
      float adjusted = minLevel + (1.0 - minLevel) * wave;

      // gamma correction
      float corrected = pow(adjusted, 2.2);

      uint8_t level = corrected * deviceSettings.ledBrightness * 25.5;

      // Brightness level
      uint8_t brightness = corrected * 255;

      // Generate color using HSV
      uint32_t color = pixels.ColorHSV(baseHue);

      // Apply brightness manually (important)
      uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
      uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
      uint8_t b = (color & 0xFF) * brightness / 255;

      pixels.fill(pixels.Color(r, g, b));
      break;
      }

    case IDLE_RAINBOW: {
      static uint16_t hue = 0;
      hue += 256;

      for(int i=0;i<LED_COUNT;i++){
        pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV(hue + (i * 65536L / LED_COUNT))));
      }
      break;
    }

    case IDLE_PULSE: {
      static float phase = 0.0;
      phase += 0.05;  // speed (lower = slower, smoother)
      float wave = (sin(phase) + 1.0) * 0.5;  // 0 to 1 smooth
      // Gamma correction (THIS is what makes it feel premium)
      float corrected = pow(wave, 2.2);
      // Scale to brightness (respects your global LED brightness)
      uint8_t level = corrected * deviceSettings.ledBrightness * 25.5;
      // Your color tone (tweakable)
      uint8_t r = level / 4;
      uint8_t g = 0;
      uint8_t b = level / 6;
      pixels.fill(pixels.Color(r, g, b));
      break;  
    }
  }

  break;
    case LED_TIMER_ALARM:
      if(toggle) pixels.fill(pixels.Color(255,0,0));
      else pixels.clear();
      break;

    case LED_REMINDER_ALARM:
      if(toggle) pixels.fill(pixels.Color(255,80,0));
      else pixels.clear();
      break;

    case LED_SUCCESS:
      pixels.fill(pixels.Color(0,255,0));
      break;
  }

  pixels.show();
}

// ================= SETUP =================

void setup(){
  initHardware();
  timerTotalMillis=((unsigned long)timerHours*3600UL+(unsigned long)timerMinutes*60UL+(unsigned long)timerSeconds)*1000UL;
  if(timerTotalMillis==0) timerTotalMillis=1000;
  timerRemainingMillis=timerTotalMillis;

  while(!bootAnimation()){ updateBuzzer(); }

  currentQuoteIdx=pickQuote();
  loadQuote();
  lcd.clear();
}

// Helper: days in month
int daysInMonth(int year, int month){
  if(month==1||month==3||month==5||month==7||month==8||month==10||month==12) return 31;
  if(month==4||month==6||month==9||month==11) return 30;
  // feb
  bool leap = (year%4==0 && (year%100!=0 || year%400==0));
  return leap ? 29 : 28;
}

void beginSettingsEdit(){
  resetSettingsEditModes();

  if(settingsIndex==9){
    DateTime n=rtc.now();
    adjustHour=n.hour();
    adjustMinute=n.minute();
  }
  else if(settingsIndex==10){
    DateTime n=rtc.now();
    adjustYear=n.year();
    adjustMonth=n.month();
    adjustDay=n.day();
  }

  enterState(STATE_SETTINGS_EDIT);
}

void adjustSettingsValue(int step){
  if(settingsIndex==4){
    if(scheduleEditField==SCHEDULE_HOUR){
      int h = (int)deviceSettings.lightsOnHour + step;
      if(h < 0) h = 23;
      if(h > 23) h = 0;
      deviceSettings.lightsOnHour = (uint8_t)h;
    } else {
      int m = (int)deviceSettings.lightsOnMinute + step;
      if(m < 0) m = 59;
      if(m > 59) m = 0;
      deviceSettings.lightsOnMinute = (uint8_t)m;
    }
    refreshLightOutputs();
  }
  else if(settingsIndex==5){
    if(scheduleEditField==SCHEDULE_HOUR){
      int h = (int)deviceSettings.lightsOffHour + step;
      if(h < 0) h = 23;
      if(h > 23) h = 0;
      deviceSettings.lightsOffHour = (uint8_t)h;
    } else {
      int m = (int)deviceSettings.lightsOffMinute + step;
      if(m < 0) m = 59;
      if(m > 59) m = 0;
      deviceSettings.lightsOffMinute = (uint8_t)m;
    }
    refreshLightOutputs();
  }
  else if(settingsIndex==9){
    if(adjustHourField) adjustHour=(adjustHour+step+24)%24;
    else adjustMinute=(adjustMinute+step+60)%60;
  }
  else if(settingsIndex==10){
    if(adjustDateField==DATE_DAY){
      int maxd = daysInMonth(adjustYear, adjustMonth);
      int d = (int)adjustDay + step;
      if(d < 1) d = maxd;
      if(d > maxd) d = 1;
      adjustDay = (uint8_t)d;
    }
    else if(adjustDateField==DATE_MONTH){
      int mo = (int)adjustMonth + step;
      if(mo < 1) mo = 12;
      if(mo > 12) mo = 1;
      adjustMonth = (uint8_t)mo;
      int maxd = daysInMonth(adjustYear, adjustMonth);
      if(adjustDay > maxd) adjustDay = maxd;
    }
    else{
      int y = (int)adjustYear + step;
      if(y < 2000) y = 2000;
      if(y > 2099) y = 2099;
      adjustYear = (uint16_t)y;
      int maxd = daysInMonth(adjustYear, adjustMonth);
      if(adjustDay > maxd) adjustDay = maxd;
    }
  }
  else if(settingsIndex==0){
    deviceSettings.brightness = !deviceSettings.brightness;
    refreshLightOutputs();
  }
  else if(settingsIndex==1){
    int val = deviceSettings.idlePreset + step;
    if(val < 0) val = IDLE_PULSE;
    if(val > IDLE_PULSE) val = IDLE_OFF;
    deviceSettings.idlePreset = val;
    idlePreset = (LedIdlePreset)val;
  }
  else if(settingsIndex==2){
    int val = deviceSettings.ledBrightness + step;
    if(val < 0) val = 0;
    if(val > 10) val = 10;
    deviceSettings.ledBrightness = val;
    pixels.setBrightness(deviceSettings.ledBrightness * 25.5);
    pixels.show();
  }
  else if(settingsIndex==3){
    deviceSettings.autoLights=!deviceSettings.autoLights;
    refreshLightOutputs();
  }
  else if(settingsIndex==6) deviceSettings.buzzer=!deviceSettings.buzzer;
  else if(settingsIndex==7) deviceSettings.quotes=!deviceSettings.quotes;
  else if(settingsIndex==8) deviceSettings.format24=!deviceSettings.format24;

  stateChanged = true;
}

void advanceSettingsField(){
  if(settingsIndex==4 || settingsIndex==5){
    scheduleEditField = (scheduleEditField==SCHEDULE_HOUR) ? SCHEDULE_MINUTE : SCHEDULE_HOUR;
  }
  else if(settingsIndex==9){
    adjustHourField=!adjustHourField;
  }
  else if(settingsIndex==10){
    if(adjustDateField==DATE_DAY) adjustDateField=DATE_MONTH;
    else if(adjustDateField==DATE_MONTH) adjustDateField=DATE_YEAR;
    else adjustDateField=DATE_DAY;
  }
  else{
    adjustSettingsValue(1);
  }
  stateChanged = true;
}

void commitSettingsEdit(){
  if(settingsIndex==9){
    DateTime current = rtc.now();
    rtc.adjust(DateTime(current.year(),current.month(),current.day(),adjustHour,adjustMinute,0));
    cachedTime = rtc.now();
  }
  else if(settingsIndex==10){
    DateTime current = rtc.now();
    rtc.adjust(DateTime(adjustYear,adjustMonth,adjustDay,current.hour(),current.minute(),0));
    cachedTime = rtc.now();
  }

  saveSettings();
  refreshLightOutputs();
  resetSettingsEditModes();
  enterState(STATE_SETTINGS_MENU);
}

void resetTimerAlarm(bool restoreDuration){
  currentLedState = LED_IDLE;
  if(restoreDuration) timerRemainingMillis=timerTotalMillis;
  enterState(STATE_TIMER_VIEW);
}

// ================= MAIN LOOP =================

void loop(){
  unsigned long now=millis();
  updateBuzzer();

  if(now-lastLightScheduleCheck>1000){
    refreshLightOutputs();
    lastLightScheduleCheck=now;
  }

  updateLEDs();

  if(currentState==STATE_TIMER_ALARM && now-lastAlarmBeep>1200){
    triggerBeep(200);
    lastAlarmBeep=now;
  }
  if(currentState==STATE_TIMER_ALARM && now-alarmStartTime>30000){
    resetTimerAlarm(false);
  }

  InputEvent ev=readInput();

  if(currentState==STATE_REMINDER_ALARM){
    updateReminderAlarm();
    if(ev==EVENT_CLICK || ev==EVENT_DOUBLE_CLICK || ev==EVENT_LONG_PRESS){
      stopReminderAlarm();
    }
    return;
  }

  if(ev==EVENT_ROTATE_CW || ev==EVENT_ROTATE_CCW){
    int step = (ev==EVENT_ROTATE_CW)?1:-1;

    switch(currentState){
      case STATE_REMINDER_LIST:{
        int newIndex = (int)reminderIndex + step;
        if(newIndex < 0) newIndex = MAX_REMINDERS - 1;
        if(newIndex >= MAX_REMINDERS) newIndex = 0;
        reminderIndex = (uint8_t)newIndex;
        stateChanged = true;
        if(deviceSettings.buzzer) triggerBeep(20);
        break;
      }

      case STATE_REMINDER_EDIT:
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
        saveReminders(); // persist while editing
        stateChanged = true;
        if(deviceSettings.buzzer) triggerBeep(20);
        break;

      case STATE_TIMER_EDIT:
        if(timerEditField==EDIT_HOURS) timerHours = constrain(timerHours + step,0,99);
        else if(timerEditField==EDIT_MINUTES) timerMinutes = constrain(timerMinutes + step,0,59);
        else timerSeconds = constrain(timerSeconds + step,0,59);

        timerTotalMillis=((unsigned long)timerHours*3600UL+(unsigned long)timerMinutes*60UL+(unsigned long)timerSeconds)*1000UL;
        if(timerTotalMillis==0) timerTotalMillis=1000;
        timerRemainingMillis=timerTotalMillis;
        stateChanged = true;
        break;

      case STATE_SETTINGS_MENU:{
        int newIndex = (int)settingsIndex + step;
        if(newIndex < 0) newIndex = SETTINGS_COUNT-1;
        if(newIndex >= SETTINGS_COUNT) newIndex = 0;
        settingsIndex = (uint8_t)newIndex;
        stateChanged = true;
        break;
      }

      case STATE_SETTINGS_EDIT:
        adjustSettingsValue(step);
        break;

      case STATE_CLOCK:
      case STATE_TIMER_VIEW:
      case STATE_STOPWATCH:
      case STATE_REMINDER_HOME:
      case STATE_SETTINGS_HOME:
        rotateMainState(step);
        if(deviceSettings.buzzer) triggerBeep(20);
        break;

      default:
        break;
    }
  }

  if(ev==EVENT_CLICK){
    switch(currentState){
      case STATE_CLOCK:
        currentQuoteIdx=pickQuote();
        loadQuote();
        break;

      case STATE_TIMER_VIEW:
        if(timerRunning){
          timerRemainingMillis=(millis()>=timerEndTime)?0:timerEndTime-millis();
          timerRunning=false;
        } else {
          timerEndTime=millis()+timerRemainingMillis;
          timerRunning=true;
        }
        break;

      case STATE_TIMER_EDIT:
        if(timerEditField==EDIT_HOURS) timerEditField=EDIT_MINUTES;
        else if(timerEditField==EDIT_MINUTES) timerEditField=EDIT_SECONDS;
        else timerEditField=EDIT_HOURS;
        stateChanged = true;
        break;

      case STATE_TIMER_ALARM:
        resetTimerAlarm(true);
        break;

      case STATE_STOPWATCH:
        stopwatchRunning=!stopwatchRunning;
        if(stopwatchRunning) stopwatchStartTime=millis()-stopwatchElapsed;
        break;

      case STATE_REMINDER_HOME:
        enterState(STATE_REMINDER_LIST);
        break;

      case STATE_REMINDER_LIST:
        reminderField = REM_EDIT_HOUR;
        enterState(STATE_REMINDER_EDIT);
        break;

      case STATE_REMINDER_EDIT:
        reminderField = (reminderField == REM_EDIT_HOUR) ? REM_EDIT_MINUTE : REM_EDIT_HOUR;
        stateChanged = true;
        break;

      case STATE_SETTINGS_HOME:
        enterState(STATE_SETTINGS_MENU);
        break;

      case STATE_SETTINGS_MENU:
        beginSettingsEdit();
        break;

      case STATE_SETTINGS_EDIT:
        advanceSettingsField();
        break;

      default:
        break;
    }

    if(deviceSettings.buzzer) triggerBeep(40);
  }

  if(ev==EVENT_DOUBLE_CLICK){
    switch(currentState){
      case STATE_REMINDER_EDIT:
        saveReminders();
        enterState(STATE_REMINDER_LIST);
        break;

      case STATE_REMINDER_LIST:
        enterState(STATE_REMINDER_HOME);
        triggerBeep(80);
        break;

      case STATE_SETTINGS_EDIT:
        commitSettingsEdit();
        break;

      case STATE_SETTINGS_MENU:
        leaveSettingsMenu();
        break;

      case STATE_SETTINGS_HOME:
        exitSettingsToClock();
        break;

      case STATE_TIMER_VIEW:
      case STATE_TIMER_EDIT:
      case STATE_TIMER_ALARM:
        timerRunning=false;
        timerRemainingMillis=timerTotalMillis;
        resetTimerAlarm(false);
        break;

      case STATE_STOPWATCH:
        stopwatchRunning=false;
        stopwatchElapsed=0;
        if(deviceSettings.buzzer) triggerBeep(120);
        break;

      default:
        break;
    }
  }

  if(ev==EVENT_LONG_PRESS){
    switch(currentState){
      case STATE_REMINDER_LIST:
        reminders[reminderIndex].active = !reminders[reminderIndex].active;
        saveReminders();
        stateChanged = true;
        break;

      case STATE_TIMER_VIEW:
        if(!timerRunning) enterState(STATE_TIMER_EDIT);
        break;

      case STATE_TIMER_EDIT:
        enterState(STATE_TIMER_VIEW);
        break;

      case STATE_TIMER_ALARM:
        resetTimerAlarm(true);
        break;

      case STATE_SETTINGS_EDIT:
        commitSettingsEdit();
        exitSettingsToClock();
        break;

      case STATE_SETTINGS_MENU:
      case STATE_SETTINGS_HOME:
        saveSettings();
        refreshLightOutputs();
        exitSettingsToClock();
        break;

      default:
        break;
    }
    if(deviceSettings.buzzer) triggerBeep(120);
  }

  if(now-lastDisplayRefresh>250 || stateChanged){
    if(stateChanged){ lcd.clear(); memset(lcdBuffer,0,sizeof(lcdBuffer)); stateChanged=false; }

    checkReminders();

    switch(currentState){
      case STATE_CLOCK:
        renderClockScreen();
        break;

      case STATE_TIMER_VIEW:
      case STATE_TIMER_EDIT:
      case STATE_TIMER_ALARM:
        renderTimerScreen();
        updateTimer();
        break;

      case STATE_STOPWATCH:
        renderStopwatchScreen();
        updateStopwatch();
        break;

      case STATE_REMINDER_HOME:
      case STATE_REMINDER_LIST:
      case STATE_REMINDER_EDIT:
        renderRemindersScreen();
        break;

      case STATE_SETTINGS_HOME:
      case STATE_SETTINGS_MENU:
      case STATE_SETTINGS_EDIT:
        renderSettingsScreen();
        break;

      case STATE_REMINDER_ALARM:
        updateReminderAlarm();
        break;
    }

    lastDisplayRefresh=now;
  }
}
