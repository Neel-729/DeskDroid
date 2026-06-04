#include "lcd_driver.h"

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <string.h>

namespace {
LiquidCrystal_I2C lcd(0x27,16,2);
char displayedRows[2][17] = {
  "                ",
  "                "
};
bool displayCacheValid = false;
LcdDriver::TimingStats lcdTimingStats = {};

void updateMax(uint32_t &currentMax, uint32_t value){
  if(value > currentMax) currentMax = value;
}

uint32_t timedSetCursor(uint8_t col, uint8_t row){
  const uint32_t startUs = micros();
  lcd.setCursor(col, row);
  const uint32_t runtimeUs = micros() - startUs;
  lcdTimingStats.setCursorUs += runtimeUs;
  lcdTimingStats.setCursorCount++;
  lcdTimingStats.lastFrameSetCursorOps++;
  updateMax(lcdTimingStats.maxSetCursorUs, runtimeUs);
  return runtimeUs;
}

uint32_t timedPrint(const char* text){
  const uint32_t startUs = micros();
  lcd.print(text);
  const uint32_t runtimeUs = micros() - startUs;
  lcdTimingStats.printUs += runtimeUs;
  lcdTimingStats.printCount++;
  lcdTimingStats.lastFramePrintOps++;
  updateMax(lcdTimingStats.maxPrintUs, runtimeUs);
  return runtimeUs;
}

uint32_t timedWrite(uint8_t value){
  const uint32_t startUs = micros();
  lcd.write(value);
  const uint32_t runtimeUs = micros() - startUs;
  lcdTimingStats.writeUs += runtimeUs;
  lcdTimingStats.writeCount++;
  lcdTimingStats.lastFrameWriteOps++;
  updateMax(lcdTimingStats.maxWriteUs, runtimeUs);
  return runtimeUs;
}

uint32_t timedClear(){
  const uint32_t startUs = micros();
  lcd.clear();
  const uint32_t runtimeUs = micros() - startUs;
  lcdTimingStats.clearUs = runtimeUs;
  lcdTimingStats.clearCount++;
  updateMax(lcdTimingStats.maxClearUs, runtimeUs);
  return runtimeUs;
}

void normalizeRow(const char* text, char destination[17]){
  if(text == nullptr) text = "";

  memset(destination, ' ', 16);
  destination[16] = '\0';
  for(uint8_t i = 0; i < 16 && text[i] != '\0'; ++i){
    destination[i] = text[i];
  }
}

void markDisplayedBlank(){
  for(uint8_t row = 0; row < 2; ++row){
    memset(displayedRows[row], ' ', 16);
    displayedRows[row][16] = '\0';
  }
  displayCacheValid = true;
}

void recordRowStats(uint8_t row, uint32_t runtimeUs, uint16_t changedChars, uint16_t runs){
  if(row == 0){
    lcdTimingStats.row1Us = runtimeUs;
    lcdTimingStats.lastRow1ChangedChars = changedChars;
    lcdTimingStats.lastRow1Runs = runs;
    updateMax(lcdTimingStats.maxRow1Us, runtimeUs);
  } else {
    lcdTimingStats.row2Us = runtimeUs;
    lcdTimingStats.lastRow2ChangedChars = changedChars;
    lcdTimingStats.lastRow2Runs = runs;
    updateMax(lcdTimingStats.maxRow2Us, runtimeUs);
  }
}

void writeRowInternal(uint8_t row, const char* text){
  if(row >= 2) return;

  const uint32_t rowStartUs = micros();
  char target[17];
  normalizeRow(text, target);

  uint16_t changedChars = 0;
  uint16_t runs = 0;

  for(uint8_t col = 0; col < 16;){
    const bool changed = !displayCacheValid || displayedRows[row][col] != target[col];
    if(!changed){
      ++col;
      continue;
    }

    const uint8_t runStart = col;
    while(col < 16 && (!displayCacheValid || displayedRows[row][col] != target[col])){
      ++col;
    }

    runs++;
    timedSetCursor(runStart, row);
    for(uint8_t i = runStart; i < col; ++i){
      timedWrite(static_cast<uint8_t>(target[i]));
      displayedRows[row][i] = target[i];
      changedChars++;
    }
  }

  displayedRows[row][16] = '\0';
  if(changedChars == 0){
    lcdTimingStats.rowSkipCount++;
    lcdTimingStats.lastFrameRowsSkipped++;
  } else {
    lcdTimingStats.rowUpdateCount++;
    lcdTimingStats.lastFrameRowsChanged++;
  }
  lcdTimingStats.lastFrameChangedChars += changedChars;
  recordRowStats(row, micros() - rowStartUs, changedChars, runs);
}

void resetFrameStats(){
  lcdTimingStats.setCursorUs = 0;
  lcdTimingStats.printUs = 0;
  lcdTimingStats.writeUs = 0;
  lcdTimingStats.row1Us = 0;
  lcdTimingStats.row2Us = 0;
  lcdTimingStats.frameUs = 0;
  lcdTimingStats.lastFrameSetCursorOps = 0;
  lcdTimingStats.lastFramePrintOps = 0;
  lcdTimingStats.lastFrameWriteOps = 0;
  lcdTimingStats.lastFrameChangedChars = 0;
  lcdTimingStats.lastFrameRowsChanged = 0;
  lcdTimingStats.lastFrameRowsSkipped = 0;
  lcdTimingStats.lastRow1ChangedChars = 0;
  lcdTimingStats.lastRow2ChangedChars = 0;
  lcdTimingStats.lastRow1Runs = 0;
  lcdTimingStats.lastRow2Runs = 0;
}

}

namespace LcdDriver {

void begin(){
  Wire.begin(21,22);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
  lcd.init();
  lcd.backlight();
  displayCacheValid = false;
}

void createBlockChar(){
  uint8_t block[8]={255,255,255,255,255,255,255,255};
  lcd.createChar(0,block);
}

void clear(){
  timedClear();
  markDisplayedBlank();
}

void writeRow(uint8_t row, const char* text){
  resetFrameStats();
  const uint32_t frameStartUs = micros();
  writeRowInternal(row, text);
  lcdTimingStats.frameUs = micros() - frameStartUs;
  lcdTimingStats.frameUpdateCount++;
  updateMax(lcdTimingStats.maxFrameUs, lcdTimingStats.frameUs);
  displayCacheValid = true;
}

void writeFrame(const char* row0, const char* row1){
  resetFrameStats();
  const uint32_t frameStartUs = micros();
  writeRowInternal(0, row0);
  writeRowInternal(1, row1);
  lcdTimingStats.frameUs = micros() - frameStartUs;
  lcdTimingStats.frameUpdateCount++;
  updateMax(lcdTimingStats.maxFrameUs, lcdTimingStats.frameUs);
  displayCacheValid = true;
}

void setBacklight(bool enabled){
  if(enabled) lcd.backlight();
  else lcd.noBacklight();
}

const TimingStats &timingStats(){
  return lcdTimingStats;
}

}
