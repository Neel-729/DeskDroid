#pragma once

#include <Arduino.h>

namespace LcdDriver {
struct TimingStats {
  uint32_t clearUs;
  uint32_t setCursorUs;
  uint32_t printUs;
  uint32_t writeUs;
  uint32_t row1Us;
  uint32_t row2Us;
  uint32_t frameUs;
  uint32_t maxClearUs;
  uint32_t maxSetCursorUs;
  uint32_t maxPrintUs;
  uint32_t maxWriteUs;
  uint32_t maxRow1Us;
  uint32_t maxRow2Us;
  uint32_t maxFrameUs;
  uint32_t clearCount;
  uint32_t setCursorCount;
  uint32_t printCount;
  uint32_t writeCount;
  uint32_t rowUpdateCount;
  uint32_t rowSkipCount;
  uint32_t frameUpdateCount;
  uint16_t lastFrameSetCursorOps;
  uint16_t lastFramePrintOps;
  uint16_t lastFrameWriteOps;
  uint16_t lastFrameChangedChars;
  uint16_t lastFrameRowsChanged;
  uint16_t lastFrameRowsSkipped;
  uint16_t lastRow1ChangedChars;
  uint16_t lastRow2ChangedChars;
  uint16_t lastRow1Runs;
  uint16_t lastRow2Runs;
};

void begin();
void createBlockChar();
void clear();
void writeRow(uint8_t row, const char* text);
void writeFrame(const char* row0, const char* row1);
void setBacklight(bool enabled);
const TimingStats &timingStats();
}
