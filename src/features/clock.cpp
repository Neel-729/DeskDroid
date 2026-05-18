#include "clock.h"

#include <Arduino.h>
#include <pgmspace.h>
#include <string.h>

#include "../drivers/rtc_driver.h"

namespace {

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

const char* days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

DateTime cachedTime;
unsigned long lastRtcUpdate = 0;
unsigned long lastQuoteScroll = 0;
unsigned long quotePauseUntil = 0;

uint8_t lastQuote = 255;
uint32_t quoteUsedMask = 0;
uint8_t quoteUsedCount = 0;

char activeQuote[96];
uint8_t currentQuoteIdx = 0;
int scrollPosition = 0;
int activeQuoteLength = 0;

char currentTimeRow[17];
char currentQuoteRow[17];

uint8_t pickQuote(){
  if(quoteUsedCount >= QUOTE_COUNT){
    quoteUsedMask = 0;
    quoteUsedCount = 0;
  }

  uint8_t q;
  do{
    q = random(QUOTE_COUNT);
  }while((quoteUsedMask & (1UL << q)) || q == lastQuote);

  quoteUsedMask |= (1UL << q);
  quoteUsedCount++;
  lastQuote = q;

  return q;
}

void loadQuote(unsigned long now){
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
  for(int i=0;i<16;i++) currentQuoteRow[i]=activeQuote[i];
  currentQuoteRow[16]='\0';
  quotePauseUntil=now+2500;
}

void updateQuote(unsigned long now, bool quotesEnabled){
  if(!quotesEnabled){
    strcpy(currentQuoteRow,"                ");
    return;
  }

  if(now<quotePauseUntil) return;
  if(now-lastQuoteScroll<=350) return;

  for(int i=0;i<16;i++){
    currentQuoteRow[i]=activeQuote[(scrollPosition+i)%activeQuoteLength];
  }
  currentQuoteRow[16]='\0';
  scrollPosition++;

  if(scrollPosition>=activeQuoteLength){
    currentQuoteIdx=pickQuote();
    loadQuote(now);
  }
  lastQuoteScroll=now;
}

void updateTime(unsigned long now, bool format24){
  if(now-lastRtcUpdate>1000){
    cachedTime=RtcDriver::now();
    lastRtcUpdate=now;
  }

  int hour=cachedTime.hour();
  if(!format24){
    if(hour==0) hour=12;
    else if(hour>12) hour-=12;
  }

  snprintf(currentTimeRow,sizeof(currentTimeRow),"%02d:%02d|%s %s %02d",
    hour,
    cachedTime.minute(),
    days[cachedTime.dayOfTheWeek()],
    months[cachedTime.month()-1],
    cachedTime.day()
  );
}

}

namespace ClockFeature {

void begin(){
  currentQuoteIdx=pickQuote();
  loadQuote(millis());
}

void nextQuote(){
  currentQuoteIdx=pickQuote();
  loadQuote(millis());
}

void update(unsigned long now, bool quotesEnabled, bool format24){
  updateTime(now, format24);
  updateQuote(now, quotesEnabled);
}

void syncToRtc(){
  cachedTime=RtcDriver::now();
  lastRtcUpdate=millis();
}

const char* timeRow(){
  return currentTimeRow;
}

const char* quoteRow(){
  return currentQuoteRow;
}

}
