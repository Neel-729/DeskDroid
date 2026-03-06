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

// ================= TIMING / UI =================
unsigned long lastDisplayRefresh = 0;
unsigned long lastQuoteScroll = 0;
unsigned long lastRTCUpdate = 0;
unsigned long beepUntil = 0;
bool timerAlarmActive = false;
unsigned long lastAlarmBeep = 0;

// ================= BUTTON / PRESS DETECTION =================
unsigned long lastButtonPress = 0;
unsigned long buttonDownTime = 0;
bool buttonHeld = false;

constexpr uint16_t BUTTON_DEBOUNCE = 250;
constexpr uint16_t LONG_PRESS = 600;
constexpr uint16_t DOUBLE_PRESS_TIME = 350;

unsigned long lastShortPressTime = 0;
bool waitingSecondPress = false;

// ================= CLOCK CACHE =================
DateTime cachedTime;

// ================= TIMER (HH:MM:SS) =================
int timerHours = 0;
int timerMinutes = 5; // default minutes
int timerSeconds = 0;
unsigned long timerEndTime = 0;
unsigned long timerRemainingMillis = 0;
bool timerRunning = false;
bool timerEditMode = false;

enum TimerEditField { EDIT_HOURS, EDIT_MINUTES, EDIT_SECONDS };
TimerEditField timerEditField = EDIT_MINUTES;

// blink for edit field
bool blinkState = true;
unsigned long lastBlink = 0;

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

// ================= CLOCK NAMES =================
const char* days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

// ================= ENCODER (state-machine quadrature) =================
int8_t readEncoder() {
  static uint8_t prevState = 0;
  static int8_t stepAccum = 0;

  prevState <<= 2;
  if (digitalRead(ENCODER_CLK)) prevState |= 0x02;
  if (digitalRead(ENCODER_DT))  prevState |= 0x01;
  prevState &= 0x0F;

  if (prevState == 0b1101 || prevState == 0b0100 || prevState == 0b0010 || prevState == 0b1011) stepAccum++;
  if (prevState == 0b1110 || prevState == 0b0111 || prevState == 0b0001 || prevState == 0b1000) stepAccum--;

  if (stepAccum >= 2) { stepAccum = 0; return +1; }
  if (stepAccum <= -2) { stepAccum = 0; return -1; }
  return 0;
}

// ================= BUZZER =================
void triggerBeep(int duration = 50) {
  digitalWrite(BUZZER_PIN, HIGH);
  beepUntil = millis() + duration;
}
void updateBuzzer() { if (digitalRead(BUZZER_PIN) == HIGH && millis() > beepUntil) digitalWrite(BUZZER_PIN, LOW); }

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
    if (i < filled) lcd.write((uint8_t)0); else lcd.print(' ');
  }
}

// ================= QUOTE SYSTEM =================
void loadQuote() {
  const char* ptr = (const char*)pgm_read_ptr(&(quotes[currentQuoteIdx]));
  strcpy_P(activeQuote, ptr);
  activeQuoteLength = strlen(activeQuote);
  if (activeQuoteLength < (int)sizeof(activeQuote) - 17) {
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

// ================= TIMER (render & update) =================
void renderTimerScreen() {
  writeRow(0, "Timer");

  // blinking for edit field
  if (millis() - lastBlink > 500) { blinkState = !blinkState; lastBlink = millis(); }

  int h = timerHours;
  int m = timerMinutes;
  int s = timerSeconds;

  if (timerEditMode && !blinkState) {
    if (timerEditField == EDIT_HOURS) h = -1;
    if (timerEditField == EDIT_MINUTES) m = -1;
    if (timerEditField == EDIT_SECONDS) s = -1;
  }

  char buf[17];
  char hbuf[3], mbuf[3], sbuf[3];
  if (h < 0) strcpy(hbuf, "  "); else snprintf(hbuf, 3, "%02d", h);
  if (m < 0) strcpy(mbuf, "  "); else snprintf(mbuf, 3, "%02d", m);
  if (s < 0) strcpy(sbuf, "  "); else snprintf(sbuf, 3, "%02d", s);

  snprintf(buf, sizeof(buf), "%s:%s:%s", hbuf, mbuf, sbuf);
  writeRow(1, buf);
}

void updateTimer() {
  // if not running, show remaining
  if (!timerRunning) {
    unsigned long remaining = timerRemainingMillis;
    int hours = remaining / 3600000;
    int minutes = (remaining / 60000) % 60;
    int seconds = (remaining / 1000) % 60;
    char buf[17];
    if (timerAlarmActive) snprintf(buf, sizeof(buf), "00:00:00 Done");
    else snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
    writeRow(1, buf);
    return;
  }

  // running
  if (millis() >= timerEndTime) {
    timerRunning = false; timerRemainingMillis = 0; timerAlarmActive = true; writeRow(1, "00:00:00 Done"); return;
  }

  unsigned long remaining = timerEndTime - millis();
  int hours = remaining / 3600000;
  int minutes = (remaining / 60000) % 60;
  int seconds = (remaining / 1000) % 60;
  char buf[17];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
  writeRow(1, buf);
}

// ================= STOPWATCH =================
void renderStopwatchScreen() {
  writeRow(0, "Stopwatch");
  if (!stopwatchRunning && stopwatchElapsed == 0) writeRow(1, "00:00.00");
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
  lcd.init(); lcd.backlight();
  uint8_t block[8] = {255,255,255,255,255,255,255,255};
  lcd.createChar(0, block); // custom solid block
  if (!rtc.begin()) { writeRow(0, "RTC ERROR"); while (true); }
  if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  pinMode(ENCODER_CLK, INPUT_PULLUP); pinMode(ENCODER_DT, INPUT_PULLUP); pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  randomSeed((uint32_t)rtc.now().unixtime());
}

// ================= SETUP =================
void setup() {
  initHardware();
  // set remaining from HH:MM:SS
  timerRemainingMillis = ((unsigned long)timerHours * 3600UL + (unsigned long)timerMinutes * 60UL + (unsigned long)timerSeconds) * 1000UL;
  writeRow(0, "   Desk Clock   "); writeRow(1, "   Booting...   ");
  delay(1500);
  currentQuoteIdx = random(0, 3); loadQuote(); lcd.clear();
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
    unsigned long delta = now - lastEncoderMove; lastEncoderMove = now;
    int step = movement;
    if (delta < 80) step *= 5; // acceleration

    if (currentState == TIMER && timerEditMode && !timerRunning) {
      // adjust selected field
      if (timerEditField == EDIT_HOURS) {
        timerHours += step; if (timerHours < 0) timerHours = 0; if (timerHours > 99) timerHours = 99;
      } else if (timerEditField == EDIT_MINUTES) {
        timerMinutes += step; if (timerMinutes < 0) timerMinutes = 0; if (timerMinutes > 59) timerMinutes = 59;
      } else {
        timerSeconds += step; if (timerSeconds < 0) timerSeconds = 0; if (timerSeconds > 59) timerSeconds = 59;
      }
      // update remaining
      timerRemainingMillis = ((unsigned long)timerHours * 3600UL + (unsigned long)timerMinutes * 60UL + (unsigned long)timerSeconds) * 1000UL;
      stateChanged = true;
    } else {
      if (movement > 0) currentState = (State)((currentState + 1) % 5); else currentState = (State)((currentState + 4) % 5);
      stateChanged = true;
    }
    triggerBeep(15);
  }

  bool pressed = digitalRead(ENCODER_SW) == LOW;
  if (pressed && !buttonHeld) { buttonHeld = true; buttonDownTime = now; }

  // show progress bar while holding
  if (pressed && buttonHeld) {
    float progress = (float)(now - buttonDownTime) / (float)LONG_PRESS; if (progress < 0) progress = 0; if (progress > 1) progress = 1;
    if (currentState == TIMER || currentState == STOPWATCH) drawProgressBar(1, progress);
  }

  // BUTTON RELEASE handling
  if (!pressed && buttonHeld) {
    unsigned long duration = now - buttonDownTime;

    if (duration > LONG_PRESS) {
      // long press: toggle edit mode for timer (only when timer not running)
      triggerBeep(120);
      if (currentState == TIMER && !timerRunning) {
        timerEditMode = !timerEditMode;
        // ensure edit field defaults to minutes when entering
        timerEditField = EDIT_MINUTES;
      }
    }
    else if (now - lastButtonPress > BUTTON_DEBOUNCE) {
      // DOUBLE PRESS detection
      if (waitingSecondPress && (now - lastShortPressTime < DOUBLE_PRESS_TIME)) {
        waitingSecondPress = false;
        // double press actions: reset timer/stopwatch
        if (currentState == TIMER) {
          timerRunning = false; timerAlarmActive = false;
          timerRemainingMillis = ((unsigned long)timerHours * 3600UL + (unsigned long)timerMinutes * 60UL + (unsigned long)timerSeconds) * 1000UL;
        }
        if (currentState == STOPWATCH) {
          stopwatchRunning = false; stopwatchElapsed = 0;
        }
        triggerBeep(120);
      }
      else {
        // single press actions
        waitingSecondPress = true; lastShortPressTime = now;

        switch (currentState) {
          case CLOCK:
            currentQuoteIdx = (currentQuoteIdx + 1) % 3; loadQuote(); break;

          case TIMER:
            if (timerAlarmActive) {
              timerAlarmActive = false; // dismiss alarm
            }
            else if (timerEditMode) {
              // in edit mode: single press cycles fields
              if (timerEditField == EDIT_HOURS) timerEditField = EDIT_MINUTES;
              else if (timerEditField == EDIT_MINUTES) timerEditField = EDIT_SECONDS;
              else timerEditField = EDIT_HOURS;
            }
            else if (timerRunning) {
              // pause
              if (millis() >= timerEndTime) timerRemainingMillis = 0; else timerRemainingMillis = timerEndTime - millis();
              timerRunning = false;
            }
            else {
              // start timer
              if (timerRemainingMillis == 0)
                timerRemainingMillis = ((unsigned long)timerHours * 3600UL + (unsigned long)timerMinutes * 60UL + (unsigned long)timerSeconds) * 1000UL;
              timerEndTime = millis() + timerRemainingMillis;
              timerRunning = true;
            }
            break;

          case STOPWATCH:
            stopwatchRunning = !stopwatchRunning;
            if (stopwatchRunning) stopwatchStartTime = millis() - stopwatchElapsed;
            break;

          default: break;
        }
      }
      lastButtonPress = now;
    }
    buttonHeld = false;
  }

  // if waiting for second press and time elapsed, clear the flag
  if (waitingSecondPress && (now - lastShortPressTime > DOUBLE_PRESS_TIME)) waitingSecondPress = false;

  // display update
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
