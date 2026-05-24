#include "lcd_driver.h"

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <string.h>

namespace {
LiquidCrystal_I2C lcd(0x27,16,2);
char lcdBuffer[2][17]={{0}};
}

namespace LcdDriver {

void begin(){
  Wire.begin(21,22);
  lcd.init();
  lcd.backlight();
}

void createBlockChar(){
  uint8_t block[8]={255,255,255,255,255,255,255,255};
  lcd.createChar(0,block);
}

void clear(){
  lcd.clear();
  memset(lcdBuffer,0,sizeof(lcdBuffer));
}

void writeRow(uint8_t row, const char* text){
  char buf[17];
  memset(buf,' ',16);
  buf[16]='\0';
  for(int i=0;i<16 && text[i]!='\0';i++) buf[i]=text[i];

  if(strncmp(lcdBuffer[row],buf,16)==0) return;
  strcpy(lcdBuffer[row],buf);
  lcd.setCursor(0,row);
  lcd.print(buf);
}

void setBacklight(bool enabled){
  if(enabled) lcd.backlight();
  else lcd.noBacklight();
}

}

