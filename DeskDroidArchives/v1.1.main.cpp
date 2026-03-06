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
bool timerAlarmActive = false;
unsigned long lastAlarmBeep = 0;

// ================= BUTTON =================
unsigned long lastButtonPress = 0;
unsigned long buttonDownTime = 0;
bool buttonHeld = false;

constexpr uint16_t BUTTON_DEBOUNCE = 250;
constexpr uint16_t LONG_PRESS = 600;

// ================= CLOCK CACHE =================
DateTime cachedTime;

// ================= TIMER =================
int timerMinutes = 5;
unsigned long timerEndTime = 0;
unsigned long timerRemainingMillis = 0;
bool timerRunning = false;
bool timerEditMode = false;

// ================= STOPWATCH =================
unsigned long stopwatchStartTime = 0;
unsigned long stopwatchElapsed = 0;
bool stopwatchRunning = false;

// ================= QUOTES =================
const char quote1[] PROGMEM = "Discipline is freedom.   ";
const char quote2[] PROGMEM = "Focus beats talent.   ";
const char quote3[] PROGMEM = "Action creates clarity.   ";
const char* const quotes[] PROGMEM = {quote1, quote2, quote3};

char activeQuote[96];
int currentQuoteIdx = 0;
int scrollPosition = 0;
int activeQuoteLength = 0;

char timeRow[17];
char quoteRow[17];

// ================= MODE LABELS (not shown) =================
// ================= CLOCK NAMES =================
const char* days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

// ================= ENCODER (state-machine quadrature) =================
int8_t readEncoder() {
  // Robust Gray-code based decoder with detent normalization
  static uint8_t prevState = 0;
  static int8_t stepAccum = 0;

  prevState <<= 2;
  if (digitalRead(ENCODER_CLK)) prevState |= 0x02;
  if (digitalRead(ENCODER_DT))  prevState |= 0x01;
  prevState &= 0x0F;

  if (prevState == 0b1101 || prevState == 0b0100 || prevState == 0b0010 || prevState == 0b1011)
    stepAccum++;
  if (prevState == 0b1110 || prevState == 0b0111 || prevState == 0b0001 || prevState == 0b1000)
    stepAccum--;

  if (stepAccum >= 2) { stepAccum = 0; return +1; }
  if (stepAccum <= -2) { stepAccum = 0; return -1; }
  return 0;
}

// ================= BUZZER =================
void triggerBeep(int duration = 50) {
  digitalWrite(BUZZER_PIN, HIGH);
  beepUntil = millis() + duration;
}
void updateBuzzer() {
  if (digitalRead(BUZZER_PIN) == HIGH && millis() > beepUntil) digitalWrite(BUZZER_PIN, LOW);
}

// ================= LCD HELPERS =================
void writeRow(int row, const char* s) {
  char buf[17];
  memset(buf, ' ', 16);
  buf[16] = '\0';
  for (int i = 0; i < 16 && s[i] != '\0'; ++i) buf[i] = s[i];
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void drawProgressBar(int row, float progress) {
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  int filled = (int)(progress * 16.0f);
  lcd.setCursor(0, row);
  for (int i = 0; i < 16; ++i) {
    if (i < filled) lcd.write((uint8_t)0);
    else lcd.print(' ');
  }
}

// ================= QUOTE SYSTEM =================
void loadQuote() {
  const char* ptr = (const char*)pgm_read_ptr(&(quotes[currentQuoteIdx]));
  strcpy_P(activeQuote, ptr);
  activeQuoteLength = strlen(activeQuote);
  if (activeQuoteLength < (int)(sizeof(activeQuote) - 17)) {
    for (int i = 0; i < 16; ++i) activeQuote[activeQuoteLength + i] = ' ';
    activeQuoteLength += 16;
    activeQuote[activeQuoteLength] = '\0';
  }
  scrollPosition = 0;
}

void updateQuoteScroll() {
  if (activeQuoteLength <= 0) return;
  if (millis() - lastQuoteScroll > 350) {
    for (int i = 0; i < 16; ++i) quoteRow[i] = activeQuote[(scrollPosition + i) % activeQuoteLength];
    quoteRow[16] = '\0';
    scrollPosition++;
    if (scrollPosition >= activeQuoteLength) scrollPosition = 0;
    lastQuoteScroll = millis();
  }
}

// ================= CLOCK =================
void updateClockTime() {
  if (millis() - lastRTCUpdate > 1000) { cachedTime = rtc.now(); lastRTCUpdate = millis(); }
  snprintf(timeRow, sizeof(timeRow), "%02d:%02d %s %02d %s",
           cachedTime.hour(), cachedTime.minute(), days[cachedTime.dayOfTheWeek()], cachedTime.day(), months[cachedTime.month() - 1]);
}
void renderClockScreen() { updateClockTime(); updateQuoteScroll(); writeRow(0, timeRow); writeRow(1, quoteRow); }

// ================= TIMER =================
void renderTimerScreen() {
  writeRow(0, "Timer");
  char buf[17];
  if (timerRunning) snprintf(buf, sizeof(buf), "Running");
  else if (timerEditMode) snprintf(buf, sizeof(buf), "Edit %02d:00", timerMinutes);
  else snprintf(buf, sizeof(buf), "%02d:00 Ready", timerMinutes);
  writeRow(1, buf);
}

void updateTimer() {
  if (!timerRunning) {
    unsigned long remaining = timerRemainingMillis;
    int minutes = remaining / 60000;
    int seconds = (remaining / 1000) % 60;
    char buf[17];
    if (timerAlarmActive) snprintf(buf, sizeof(buf), "00:00 Done");
    else snprintf(buf, sizeof(buf), "%02d:%02d Paused", minutes, seconds);
    writeRow(1, buf);
    return;
  }
  if (millis() >= timerEndTime) {
    timerRunning = false;
    timerRemainingMillis = 0;
    timerAlarmActive = true;
    writeRow(1, "00:00 Done");
    return;
  }
  unsigned long remaining = timerEndTime - millis();
  int minutes = remaining / 60000;
  int seconds = (remaining / 1000) % 60;
  char buf[17];
  snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);
  writeRow(1, buf);
}

// ================= STOPWATCH =================
void renderStopwatchScreen() {
  writeRow(0, "Stopwatch");
  if (!stopwatchRunning && stopwatchElapsed == 0) writeRow(1, "Ready");
  else if (!stopwatchRunning) writeRow(1, "Paused");
}

void updateStopwatch() {
  if (stopwatchRunning) stopwatchElapsed = millis() - stopwatchStartTime;
  unsigned long t = stopwatchElapsed;
  int hours = t / 3600000;
  int minutes = (t / 60000) % 60;
  int seconds = (t / 1000) % 60;
  int ms = (t % 1000) / 10;
  char buf[17];
  if (hours > 0) snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
  else snprintf(buf, sizeof(buf), "%02d:%02d.%02d", minutes, seconds, ms);
  writeRow(1, buf);
}

// ================= HARDWARE INIT =================
void initHardware() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  uint8_t block[8] = {255, 255, 255, 255, 255, 255, 255, 255};
  lcd.createChar(0, block);
  if (!rtc.begin()) { writeRow(0, "RTC ERROR"); while (true); }
  if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  randomSeed((uint32_t)rtc.now().unixtime());
}

// ================= SETUP =================
void setup() {
  initHardware();
  timerRemainingMillis = (unsigned long)timerMinutes * 60000UL;
  writeRow(0, "   Desk Clock   ");
  writeRow(1, "   Booting...   ");
  delay(1500);
  currentQuoteIdx = random(0, 3);
  loadQuote();
  lcd.clear();
}

// ================= MAIN LOOP =================
void loop() {
  unsigned long now = millis();
  updateBuzzer();

  // alarm repeat beeps
  if (timerAlarmActive) {
    if (millis() - lastAlarmBeep > 1200) { triggerBeep(200); lastAlarmBeep = millis(); }
  }

  // encoder read + acceleration
  static unsigned long lastEncoderMove = 0;
  int movement = readEncoder();
  if (movement != 0) {
    unsigned long delta = now - lastEncoderMove;
    lastEncoderMove = now;
    int step = movement;
    if (delta < 80) step *= 5; // acceleration
    if (currentState == TIMER && timerEditMode && !timerRunning) {
      timerMinutes += step; if (timerMinutes < 1) timerMinutes = 1; if (timerMinutes > 99) timerMinutes = 99;
      timerRemainingMillis = (unsigned long)timerMinutes * 60000UL;
      stateChanged = true;
    } else {
      if (movement > 0) currentState = (State)((currentState + 1) % 5);
      else currentState = (State)((currentState + 4) % 5);
      stateChanged = true;
    }
    triggerBeep(15);
  }

  bool pressed = digitalRead(ENCODER_SW) == LOW;
  if (pressed && !buttonHeld) { buttonHeld = true; buttonDownTime = now; }

  // show progress bar while holding
  if (pressed && buttonHeld) {
    float progress = (float)(now - buttonDownTime) / (float)LONG_PRESS;
    if (currentState == TIMER || currentState == STOPWATCH) drawProgressBar(1, progress);
  }

  if (!pressed && buttonHeld) {
    unsigned long duration = now - buttonDownTime;
    if (duration > LONG_PRESS) {
      triggerBeep(120);
      if (currentState == STOPWATCH) {
        stopwatchRunning = false; stopwatchElapsed = 0;
      }
      if (currentState == TIMER) {
        if (timerRunning) {
          // reset when running
          timerRunning = false;
          timerRemainingMillis = (unsigned long)timerMinutes * 60000UL;
        } else {
          // start when not running
          timerEndTime = millis() + timerRemainingMillis;
          timerRunning = true;
        }
      }
    } else if (now - lastButtonPress > BUTTON_DEBOUNCE) {
      // short press
      switch (currentState) {
        case CLOCK: currentQuoteIdx = (currentQuoteIdx + 1) % 3; loadQuote(); break;
        case TIMER:
          if (timerAlarmActive) timerAlarmActive = false;
          else if (timerRunning) { if (millis() >= timerEndTime) timerRemainingMillis = 0; else timerRemainingMillis = timerEndTime - millis(); timerRunning = false; }
          else timerEditMode = !timerEditMode;
          break;
        case STOPWATCH:
          stopwatchRunning = !stopwatchRunning; if (stopwatchRunning) stopwatchStartTime = millis() - stopwatchElapsed; break;
        default: break;
      }
      lastButtonPress = now;
    }
    buttonHeld = false;
    triggerBeep(40);
  }

  if (now - lastDisplayRefresh > 250 || stateChanged) {
    if (stateChanged) { lcd.clear(); stateChanged = false; }
    switch (currentState) {
      case CLOCK: renderClockScreen(); break;
      case TIMER: renderTimerScreen(); updateTimer(); break;
      case STOPWATCH: renderStopwatchScreen(); updateStopwatch(); break;
      case REMINDERS: writeRow(0, "Reminders"); writeRow(1, "Coming Soon"); break;
      case SETTINGS: writeRow(0, "Settings"); writeRow(1, "Coming Soon"); break;
    }
    lastDisplayRefresh = now;
  }
}
