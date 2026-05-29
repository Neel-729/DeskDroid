#include "lcd_driver.h"

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <string.h>

namespace {
LiquidCrystal_I2C lcd(0x27,16,2);
char lcdBuffer[2][17]={{0}};
unsigned long lastForcedRefreshMs = 0;
bool forceRefresh = true;

constexpr unsigned long ForcedRefreshIntervalMs = 2000;

bool refreshDue(){
  const unsigned long now = millis();
  if(forceRefresh || now - lastForcedRefreshMs >= ForcedRefreshIntervalMs){
    forceRefresh = true;
    lastForcedRefreshMs = now;
    return true;
  }
  return false;
}
}

namespace LcdDriver {

void begin(){
  Wire.begin(21,22);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
  lcd.init();
  lcd.backlight();
  forceRefresh = true;
  lastForcedRefreshMs = millis();
}

void createBlockChar(){
  uint8_t block[8]={255,255,255,255,255,255,255,255};
  lcd.createChar(0,block);
}

void clear(){
  lcd.clear();
  memset(lcdBuffer,0,sizeof(lcdBuffer));
  forceRefresh = true;
}

void writeRow(uint8_t row, const char* text){
  if(row >= 2) return;
  if(text == nullptr) text = "";

  char buf[17];
  memset(buf,' ',16);
  buf[16]='\0';
  for(int i=0;i<16 && text[i]!='\0';i++) buf[i]=text[i];

  const bool shouldForceRefresh = refreshDue();
  if(!shouldForceRefresh && strncmp(lcdBuffer[row],buf,16)==0) return;
  strcpy(lcdBuffer[row],buf);
  lcd.setCursor(0,row);
  lcd.print(buf);
  if(row == 1) forceRefresh = false;
}

void setBacklight(bool enabled){
  if(enabled) lcd.backlight();
  else lcd.noBacklight();
}

}
