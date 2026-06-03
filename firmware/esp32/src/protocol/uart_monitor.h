#pragma once

#include <Arduino.h>

#include "config.h"

enum class UartDebugMode : uint8_t {
  Off = 0,
  Normal = 1,
  Verbose = 2,
  Hex = 3,
};

struct UartMonitorStats {
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
  uint32_t txBytes = 0;
  uint32_t rxBytes = 0;
  uint32_t crcErrors = 0;
  uint32_t timeoutErrors = 0;
  uint32_t resyncCount = 0;
  uint32_t malformedPackets = 0;
};

struct UartCaptureRecord {
  uint32_t timestampMs = 0;
  bool tx = false;
  uint16_t length = 0;
  char packet[Config::Esp8266MaxPacketSize + 3] = {};
};

namespace UartTrafficMonitor {

void begin();
void setMode(UartDebugMode mode);
UartDebugMode mode();
void recordTx(const char* packet, uint32_t nowMs);
void recordRx(const char* packet, uint32_t nowMs);
void recordCrcError(uint32_t nowMs);
void recordTimeout(uint32_t nowMs);
void recordResync(uint32_t nowMs);
void recordMalformed(const char* reason, uint32_t nowMs);
void dumpRecent(const char* reason, uint32_t nowMs);
const UartMonitorStats& stats();

}  // namespace UartTrafficMonitor
