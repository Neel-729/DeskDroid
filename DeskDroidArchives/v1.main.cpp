#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <pgmspace.h>

// ================= PIN DEFINITIONS =================
constexpr uint8_t ENCODER_CLK = 32;
constexpr uint8_t ENCODER_DT  = 33;
constexpr uint8_t ENCODER_SW  = 25;
constexpr uint8_t BUZZER_PIN  = 26;

// ================= HARDWARE OBJECTS =================
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

// ================= STATE MACHINE =================
enum State { CLOCK, TIMER, STOPWATCH, REMINDERS, SETTINGS };
State currentState = CLOCK;
bool stateChanged = true;

// ================= TIMING =================
unsigned long lastDisplayRefresh = 0;
unsigned long lastQuoteScroll = 0;
unsigned long lastRTCUpdate = 0;
unsigned long beepUntil = 0;

// ================= BUTTON =================
unsigned long lastButtonPress = 0;
unsigned long buttonDownTime = 0;
bool buttonHeld = false;

constexpr uint16_t BUTTON_DEBOUNCE = 250;
constexpr uint16_t LONG_PRESS = 800;

// ================= CLOCK CACHE =================
DateTime cachedTime;

// ================= TIMER =================
int timerMinutes = 5;
unsigned long timerStartTime = 0;
bool timerRunning = false;
bool timerEditMode = false;

// ================= STOPWATCH =================
unsigned long stopwatchStartTime = 0;
unsigned long stopwatchElapsed = 0;
bool stopwatchRunning = false;

// ================= QUOTES =================
const char quote1[] PROGMEM = "Discipline is freedom.   ";
const char quote2[] PROGMEM = "Focus beats talent.";
const char quote3[] PROGMEM = "Action creates clarity.   ";

const char* const quotes[] PROGMEM = {quote1, quote2, quote3};

char activeQuote[96];
int currentQuoteIdx = 0;
int scrollPosition = 0;
int activeQuoteLength = 0;

char timeRow[17];
char quoteRow[17];

// ================= CLOCK NAMES =================
const char* days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

// ================= PROFESSIONAL ROTARY ENCODER =================
// State machine quadrature decoder used in commercial firmware

// Rotary encoder state machine with detent normalization
// Many KY-040 encoders produce 2 steps per detent, so we normalize to 1
int8_t readEncoder() {

  static uint8_t prevState = 0;
  static int8_t stepAccum = 0;

  prevState <<= 2;

  if (digitalRead(ENCODER_CLK)) prevState |= 0x02;
  if (digitalRead(ENCODER_DT))  prevState |= 0x01;

  prevState &= 0x0F;

  if (prevState == 0b1101 ||
      prevState == 0b0100 ||
      prevState == 0b0010 ||
      prevState == 0b1011)
      stepAccum++;

  if (prevState == 0b1110 ||
      prevState == 0b0111 ||
      prevState == 0b0001 ||
      prevState == 0b1000)
      stepAccum--;

  // normalize to 1 step per detent
  if (stepAccum >= 2) {
    stepAccum = 0;
    return +1;
  }

  if (stepAccum <= -2) {
    stepAccum = 0;
    return -1;
  }

  return 0;
}

// ================= BUZZER =================
void triggerBeep(int duration = 50) {
  digitalWrite(BUZZER_PIN, HIGH);
  beepUntil = millis() + duration;
}

void updateBuzzer() {
  if (digitalRead(BUZZER_PIN) == HIGH && millis() > beepUntil)
    digitalWrite(BUZZER_PIN, LOW);
}

// ================= LCD HELPERS =================
void writeRow(int row, const char* s) {

  char buf[17];

  memset(buf,' ',16);

  buf[16]='\0';

  for(int i=0;i<16 && s[i]!='\0';i++)
    buf[i]=s[i];

  lcd.setCursor(0,row);

  lcd.print(buf);
}

// ================= QUOTE SYSTEM =================
void loadQuote() {

  const char* ptr = (const char*)pgm_read_ptr(&(quotes[currentQuoteIdx]));

  strcpy_P(activeQuote, ptr);

  activeQuoteLength = strlen(activeQuote);

  for(int i=0;i<16;i++)
    activeQuote[activeQuoteLength+i]=' ';

  activeQuoteLength += 16;

  activeQuote[activeQuoteLength]='\0';

  scrollPosition = 0;
}

void updateQuoteScroll() {

  if(millis()-lastQuoteScroll>350) {

    for(int i=0;i<16;i++)
      quoteRow[i]=activeQuote[(scrollPosition+i)%activeQuoteLength];

    quoteRow[16]='\0';

    scrollPosition++;

    if(scrollPosition>=activeQuoteLength)
      scrollPosition=0;

    lastQuoteScroll=millis();
  }
}

// ================= CLOCK =================
void updateClockTime() {

  if(millis()-lastRTCUpdate>1000) {

    cachedTime = rtc.now();

    lastRTCUpdate = millis();
  }

  snprintf(timeRow,sizeof(timeRow),"%02d:%02d %s %02d %s",
           cachedTime.hour(),
           cachedTime.minute(),
           days[cachedTime.dayOfTheWeek()],
           cachedTime.day(),
           months[cachedTime.month()-1]);
}

void renderClockScreen() {

  updateClockTime();

  updateQuoteScroll();

  writeRow(0,timeRow);

  writeRow(1,quoteRow);
}

// ================= TIMER =================
void renderTimerScreen() {

  writeRow(0,"Timer");

  char buf[17];

  snprintf(buf,sizeof(buf),"%02d:00",timerMinutes);

  writeRow(1,buf);
}

void updateTimer() {

  if(!timerRunning) return;

  unsigned long elapsed = millis()-timerStartTime;

  long remaining = (timerMinutes*60000L)-elapsed;

  if(remaining<=0) {

    timerRunning=false;

    remaining=0;

    triggerBeep(300);
  }

  int minutes = remaining/60000;

  int seconds = (remaining/1000)%60;

  char buf[17];

  snprintf(buf,sizeof(buf),"%02d:%02d",minutes,seconds);

  writeRow(1,buf);
}

// ================= STOPWATCH =================
void renderStopwatchScreen() {

  writeRow(0,"Stopwatch");
}

void updateStopwatch() {

  if(stopwatchRunning)
    stopwatchElapsed = millis()-stopwatchStartTime;

  unsigned long t = stopwatchElapsed;

  int minutes = t/60000;

  int seconds = (t/1000)%60;

  int ms = (t%1000)/10;

  char buf[17];

  snprintf(buf,sizeof(buf),"%02d:%02d.%02d",minutes,seconds,ms);

  writeRow(1,buf);
}

// ================= HARDWARE INIT =================
void initHardware() {

  Serial.begin(115200);

  Wire.begin(21,22);

  lcd.init();

  lcd.backlight();

  if(!rtc.begin()) {

    writeRow(0,"RTC ERROR");

    while(true);
  }

  rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));

  pinMode(ENCODER_CLK,INPUT_PULLUP);

  pinMode(ENCODER_DT,INPUT_PULLUP);

  pinMode(ENCODER_SW,INPUT_PULLUP);

  pinMode(BUZZER_PIN,OUTPUT);

  randomSeed(rtc.now().unixtime());
}

// ================= SETUP =================
void setup() {

  initHardware();

  writeRow(0,"   Desk Clock   ");

  writeRow(1,"   Booting...   ");

  delay(1500);

  currentQuoteIdx = random(0,3);

  loadQuote();

  lcd.clear();
}

// ================= MAIN LOOP =================
void loop() {

  unsigned long now = millis();

  updateBuzzer();

  // ===== Encoder movement =====
  int movement = readEncoder();

  if (movement != 0) {

    if (currentState == TIMER && timerEditMode && !timerRunning) {

      timerMinutes += movement;

      if (timerMinutes < 1) timerMinutes = 1;
      if (timerMinutes > 99) timerMinutes = 99;

      stateChanged = true;

    } else {

      if (movement > 0)
        currentState = (State)((currentState + 1) % 5);
      else
        currentState = (State)((currentState + 4) % 5);

      stateChanged = true;
    }

    triggerBeep(15);
  }


  // ===== Button handling =====
  bool pressed = digitalRead(ENCODER_SW) == LOW;

  if (pressed && !buttonHeld) {
    buttonHeld = true;
    buttonDownTime = now;
  }

  if (!pressed && buttonHeld) {

    unsigned long duration = now - buttonDownTime;

    if (duration > LONG_PRESS) {

      // Long press actions
      if (currentState == STOPWATCH) {
        stopwatchRunning = false;
        stopwatchElapsed = 0;
      }

      if (currentState == TIMER) {
        timerRunning = true;
        timerStartTime = millis();
      }

    } else if (now - lastButtonPress > BUTTON_DEBOUNCE) {

      // Short press actions
      switch (currentState) {

        case CLOCK:
          currentQuoteIdx = (currentQuoteIdx + 1) % 3;
          loadQuote();
          break;

        case TIMER:
          if (!timerRunning)
            timerEditMode = !timerEditMode;
          else
            timerRunning = false;
          break;

        case STOPWATCH:
          stopwatchRunning = !stopwatchRunning;
          if (stopwatchRunning)
            stopwatchStartTime = millis() - stopwatchElapsed;
          break;

        default:
          break;
      }

      lastButtonPress = now;
    }

    buttonHeld = false;
    triggerBeep(40);
  }


  // ===== Display update =====
  if (now - lastDisplayRefresh > 200 || stateChanged) {

    if (stateChanged) {
      lcd.clear();
      stateChanged = false;
    }

    switch (currentState) {

      case CLOCK:
        renderClockScreen();
        break;

      case TIMER:
        renderTimerScreen();
        updateTimer();
        break;

      case STOPWATCH:
        renderStopwatchScreen();
        updateStopwatch();
        break;

      case REMINDERS:
        writeRow(0, "Reminders");
        writeRow(1, "Coming Soon");
        break;

      case SETTINGS:
        writeRow(0, "Settings");
        writeRow(1, "Coming Soon");
        break;
    }

    lastDisplayRefresh = now;
  }
}

