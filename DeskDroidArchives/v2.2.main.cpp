#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <pgmspace.h>
#include <Preferences.h>
#include <string.h>

#define FirmwareVersion "2.3"

constexpr uint8_t ENCODER_CLK = 32;
constexpr uint8_t ENCODER_DT  = 33;
constexpr uint8_t ENCODER_SW  = 25;
constexpr uint8_t BUZZER_PIN  = 26;

LiquidCrystal_I2C lcd(0x27,16,2);
RTC_DS1307 rtc;
Preferences prefs;

// ================= STATES =================

enum State { CLOCK, TIMER, STOPWATCH, REMINDERS, SETTINGS };
State currentState = CLOCK;
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

// ================= TIMERS =================

unsigned long lastDisplayRefresh = 0;
unsigned long lastQuoteScroll = 0;
unsigned long quotePauseUntil = 0;
unsigned long lastRTCUpdate = 0;
unsigned long beepUntil = 0;

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
bool timerEditMode=false;
bool timerAlarmActive=false;

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
};

Settings deviceSettings;

const char* settingsMenu[]={
  "Backlight",
  "Buzzer",
  "Quotes",
  "Time Format",
  "Adjust Time",
  "About"
};

const uint8_t SETTINGS_COUNT=6;
uint8_t settingsIndex=0;
bool settingsEditing=false;
bool timeAdjustMode=false;
uint8_t adjustHour=0;
uint8_t adjustMinute=0;
bool adjustHourField=true;
bool settingsMenuActive=false;
unsigned long rtcSyncMessageUntil=0;

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

 for(int i=0;i<16 && s[i]!='\0';i++)
  buf[i]=s[i];

 if(strncmp(lcdBuffer[row],buf,16)==0) return;

 strcpy(lcdBuffer[row],buf);

 lcd.setCursor(0,row);
 lcd.print(buf);
}

// ================= BOOT =================

bool bootAnimation(){

 static uint8_t stage=0;
 static unsigned long lastStep=0;

 const char* frames[]={
  "Booting",
  "Booting.",
  "Booting..",
  "Booting...",
  "Booting....",
  "Booting...",
  "Booting....",
  "Booting....."
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
 if(activeQuoteLength+pad>=sizeof(activeQuote)-1)
   pad=sizeof(activeQuote)-1-activeQuoteLength;

 for(int i=0;i<pad;i++)
   activeQuote[activeQuoteLength+i]=' ';

 activeQuoteLength+=pad;
 activeQuote[activeQuoteLength]='\0';

 scrollPosition=0;

 for(int i=0;i<16;i++)
  quoteRow[i]=activeQuote[i];

 quoteRow[16]='\0';

 quotePauseUntil=millis()+2500;
}

void updateQuoteScroll(){

 if(!deviceSettings.quotes){
  strcpy(quoteRow,"                ");
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

 snprintf(timeRow,sizeof(timeRow),"%02d:%02d %s %02d %s",
  hour,
  cachedTime.minute(),
  months[cachedTime.month()-1],
  cachedTime.day(),
  days[cachedTime.dayOfTheWeek()]);
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
}

void renderSettingsScreen(){

 if(rtcSyncMessageUntil && millis()<rtcSyncMessageUntil){
  writeRow(0,"RTC Synced");
  writeRow(1,"Done");
  return;
 }

 if(!settingsMenuActive){

  writeRow(0,"Settings");
  writeRow(1,"Enter");

 }
 else if(!settingsEditing){

  writeRow(0,"Settings");

  char buf[17];
  snprintf(buf,sizeof(buf),"> %s",settingsMenu[settingsIndex]);

  writeRow(1,buf);

 } else{

  switch(settingsIndex){

   case 0:{
    writeRow(0,"Backlight");
    writeRow(1,deviceSettings.brightness?"ON":"OFF");
    break;}

   case 1:
    writeRow(0,"Buzzer");
    writeRow(1,deviceSettings.buzzer?"ON":"OFF");
    break;

   case 2:
    writeRow(0,"Quotes");
    writeRow(1,deviceSettings.quotes?"ON":"OFF");
    break;

   case 3:
    writeRow(0,"Time Format");
    writeRow(1,deviceSettings.format24?"24H":"12H");
    break;

   case 4:{
    writeRow(0,"Adjust Time");

    if(millis()-lastBlink>500){
      blinkState=!blinkState;
      lastBlink=millis();
    }

    int h=adjustHour;
    int m=adjustMinute;

    if(timeAdjustMode && !blinkState){
      if(adjustHourField) h=-1;
      else m=-1;
    }

    char hbuf[3];
    char mbuf[3];

    if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,3,"%02d",h);
    if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,3,"%02d",m);

    char buf[17];
    snprintf(buf,sizeof(buf),"%s:%s",hbuf,mbuf);

    writeRow(1,buf);
    break;}

   case 5:
    writeRow(0,"DeskDroid");
    writeRow(1,"v2.3");
    break;

  }

 }
}

// ================= TIMER =================

void renderTimerScreen(){

 if(timerAlarmActive){

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

 if(timerEditMode){

  writeRow(0,"Timer > Edit");

  if(millis()-lastBlink>500){
    blinkState=!blinkState;
    lastBlink=millis();
  }

  int h=timerHours;
  int m=timerMinutes;
  int s=timerSeconds;

  if(!blinkState){
    if(timerEditField==EDIT_HOURS) h=-1;
    if(timerEditField==EDIT_MINUTES) m=-1;
    if(timerEditField==EDIT_SECONDS) s=-1;
  }

  char hbuf[3];
  char mbuf[3];
  char sbuf[3];

  if(h<0) strcpy(hbuf,"  "); else snprintf(hbuf,3,"%02d",h);
  if(m<0) strcpy(mbuf,"  "); else snprintf(mbuf,3,"%02d",m);
  if(s<0) strcpy(sbuf,"  "); else snprintf(sbuf,3,"%02d",s);

  char buf[17];
  snprintf(buf,sizeof(buf),"%s:%s:%s",hbuf,mbuf,sbuf);

  writeRow(1,buf);
  return;
 }

 if(timerRunning){

  if(millis()-lastBlink>500){
    blinkState=!blinkState;
    lastBlink=millis();
  }

  if(blinkState)
    writeRow(0,"Timer Running");
  else
    writeRow(0,"Timer        ");

 }else{

  if(timerRemainingMillis==timerTotalMillis)
    writeRow(0,"Timer");
  else
    writeRow(0,"Timer Paused");
 }
}

void updateTimer(){

 if(timerAlarmActive) return;

 unsigned long remaining;

 if(timerRunning){

  if(millis()>=timerEndTime){
    timerRunning=false;
    timerRemainingMillis=0;
    timerAlarmActive=true;
    alarmStartTime=millis();
    return;
  }

  remaining=timerEndTime-millis();

 }else{

  remaining=timerRemainingMillis;
 }

 if(!timerEditMode){

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

 if(!stopwatchRunning && stopwatchElapsed==0)
  writeRow(1,"00:00.00");
 else if(!stopwatchRunning)
  writeRow(1,"Paused");
}

void updateStopwatch(){

 if(stopwatchRunning)
  stopwatchElapsed = (unsigned long)(millis() - stopwatchStartTime);

 unsigned long t=stopwatchElapsed;

 int minutes=(t/60000)%60;
 int seconds=(t/1000)%60;
 int ms=(t%1000)/10;

 char buf[17];
 snprintf(buf,sizeof(buf),"%02d:%02d.%02d",minutes,seconds,ms);

 writeRow(1,buf);
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
 if(deviceSettings.brightness) lcd.backlight(); else lcd.noBacklight();

 randomSeed(esp_random());
}

// ================= INPUT ENGINE =================

InputEvent readInput(){

 int movement=readEncoder();

 if(movement>0) return EVENT_ROTATE_CW;
 if(movement<0) return EVENT_ROTATE_CCW;

 bool pressed=digitalRead(ENCODER_SW)==LOW;
 unsigned long now=millis();

 if(pressed && !buttonDown){
  buttonDown=true;
  buttonDownTime=now;
 }

 if(!pressed && buttonDown){

  unsigned long duration=now-buttonDownTime;

  buttonDown=false;

  if(duration>LONG_PRESS)
   return EVENT_LONG_PRESS;

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

// ================= SETUP =================

void setup(){

 initHardware();

 timerTotalMillis=((unsigned long)timerHours*3600UL+(unsigned long)timerMinutes*60UL+(unsigned long)timerSeconds)*1000UL;
   if(timerTotalMillis==0) timerTotalMillis=1000;
 timerRemainingMillis=timerTotalMillis;

 while(!bootAnimation()){
  updateBuzzer();
 }

 currentQuoteIdx=pickQuote();
 loadQuote();

 lcd.clear();
}

// ================= MAIN LOOP =================

void loop(){

 unsigned long now=millis();
 updateBuzzer();

 if(timerAlarmActive && now-lastAlarmBeep>1200){
  triggerBeep(200);
  lastAlarmBeep=now;
 }

 if(timerAlarmActive && now-alarmStartTime>30000){
  timerAlarmActive=false;
 }

 InputEvent ev=readInput();

 if(ev==EVENT_ROTATE_CW || ev==EVENT_ROTATE_CCW){

  int step=(ev==EVENT_ROTATE_CW)?1:-1;

  if(currentState==TIMER && timerEditMode && !timerRunning){

   if(timerEditField==EDIT_HOURS){
     timerHours = constrain(timerHours + step,0,99);
   } else if(timerEditField==EDIT_MINUTES){
     timerMinutes = constrain(timerMinutes + step,0,59);
   } else {
     timerSeconds = constrain(timerSeconds + step,0,59);
   }

   timerTotalMillis=((unsigned long)timerHours*3600UL+(unsigned long)timerMinutes*60UL+(unsigned long)timerSeconds)*1000UL;
   if(timerTotalMillis==0) timerTotalMillis=1000;
   timerRemainingMillis=timerTotalMillis;

  } else if(currentState==SETTINGS && settingsMenuActive && settingsEditing){

   if(settingsIndex==4 && settingsEditing && timeAdjustMode){
     if(adjustHourField){ adjustHour=(adjustHour+step+24)%24; }
     else { adjustMinute=(adjustMinute+step+60)%60; }
   }
   else if(settingsIndex==0){
     deviceSettings.brightness = !deviceSettings.brightness;
     if(deviceSettings.brightness) lcd.backlight();
     else lcd.noBacklight();
   }
   else if(settingsIndex==1){ deviceSettings.buzzer=!deviceSettings.buzzer; }
   else if(settingsIndex==2){ deviceSettings.quotes=!deviceSettings.quotes; }
   else if(settingsIndex==3){ deviceSettings.format24=!deviceSettings.format24; }

  } else if(currentState==SETTINGS && settingsMenuActive && !settingsEditing){


   int newIndex = (int)settingsIndex + step;
   if(newIndex < 0) newIndex = SETTINGS_COUNT-1;
   if(newIndex >= SETTINGS_COUNT) newIndex = 0;
   settingsIndex = (uint8_t)newIndex;

  } else {

   if(ev==EVENT_ROTATE_CW)
    currentState=(State)((currentState+1)%5);
   else
    currentState=(State)((currentState+4)%5);

   stateChanged=true;
   if(deviceSettings.buzzer) triggerBeep(20);
  }
 }

 if(ev==EVENT_CLICK){

  if(currentState==SETTINGS){

    if(!settingsMenuActive){
      settingsMenuActive=true;
    }

    else if(settingsIndex==4){

      if(!settingsEditing){
        DateTime n=rtc.now();
        adjustHour=n.hour();
        adjustMinute=n.minute();
        adjustHourField=true;
        settingsEditing=true;
        timeAdjustMode=true;
      }

      else{
        adjustHourField=!adjustHourField;
      }

    }

    else{
      settingsEditing=!settingsEditing;
      if(!settingsEditing) saveSettings();
    }

  }

  else if(currentState==CLOCK){
   currentQuoteIdx=pickQuote(); loadQuote();
  }

  else if(currentState==TIMER){

   if(timerAlarmActive){ timerAlarmActive=false; timerRemainingMillis=timerTotalMillis; }
   else if(timerEditMode){ if(timerEditField==EDIT_HOURS) timerEditField=EDIT_MINUTES; else if(timerEditField==EDIT_MINUTES) timerEditField=EDIT_SECONDS; else timerEditField=EDIT_HOURS; }
   else if(timerRunning){ timerRemainingMillis=(millis()>=timerEndTime)?0:timerEndTime-millis(); timerRunning=false; }
   else { timerEndTime=millis()+timerRemainingMillis; timerRunning=true; }
  }

  else if(currentState==STOPWATCH){
   stopwatchRunning=!stopwatchRunning; if(stopwatchRunning) stopwatchStartTime=millis()-stopwatchElapsed;
  }

  if(deviceSettings.buzzer) triggerBeep(40);
 }

 if(ev==EVENT_DOUBLE_CLICK){

  if(currentState==SETTINGS && settingsIndex==4 && settingsEditing){

    rtc.adjust(DateTime(cachedTime.year(),cachedTime.month(),cachedTime.day(),adjustHour,adjustMinute,0));

    settingsEditing=false;
    timeAdjustMode=false;

  }

  else{

    if(currentState==TIMER){ timerRunning=false; timerAlarmActive=false; timerRemainingMillis=timerTotalMillis; }

    if(currentState==STOPWATCH){ stopwatchRunning=false; stopwatchElapsed=0; }

  }

  if(deviceSettings.buzzer) triggerBeep(120);
 }

 if(ev==EVENT_LONG_PRESS){

  if(currentState==TIMER && !timerRunning) timerEditMode=!timerEditMode;

  else if(currentState==SETTINGS){
    settingsEditing=false;
    settingsMenuActive=false;
    stateChanged=true;
  }

  if(deviceSettings.buzzer) triggerBeep(120);
 }

 if(now-lastDisplayRefresh>250 || stateChanged){

  if(stateChanged){ lcd.clear(); memset(lcdBuffer,0,sizeof(lcdBuffer)); stateChanged=false; }

  switch(currentState){
   case CLOCK: renderClockScreen(); break;
   case TIMER: renderTimerScreen(); updateTimer(); break;
   case STOPWATCH: renderStopwatchScreen(); updateStopwatch(); break;
   case REMINDERS: writeRow(0,"Reminders"); writeRow(1,"Coming Soon"); break;
   case SETTINGS: renderSettingsScreen(); break;
  }

  lastDisplayRefresh=now;
 }
}
