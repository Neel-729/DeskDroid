#include "uart_monitor.h"

#include <string.h>

#include "../core/logging.h"

namespace {

constexpr uint8_t CaptureSize = 100;
constexpr uint32_t DumpRateLimitMs = 5000;

UartMonitorStats monitorStats;
UartCaptureRecord capture[CaptureSize];
uint8_t captureHead = 0;
uint8_t captureCount = 0;
UartDebugMode currentMode = static_cast<UartDebugMode>(DESKDROID_UART_MONITOR_MODE);
RateLimitedLog dumpRate;

const char* directionName(bool tx){
  return tx ? "TX" : "RX";
}

const char* commandName(const char* packet){
  static char command[18];
  command[0] = '\0';
  if(packet == nullptr) return command;

  const char* start = packet[0] == '<' ? packet + 1 : packet;
  uint8_t index = 0;
  while(start[index] != '\0' && start[index] != '|' && start[index] != '>' &&
        index < sizeof(command) - 1){
    command[index] = start[index];
    index++;
  }
  command[index] = '\0';
  return command;
}

void store(bool tx, const char* packet, uint32_t nowMs){
  if(packet == nullptr) return;

  UartCaptureRecord &record = capture[captureHead];
  record.timestampMs = nowMs;
  record.tx = tx;
  record.length = strlen(packet);
  strncpy(record.packet, packet, sizeof(record.packet) - 1);
  record.packet[sizeof(record.packet) - 1] = '\0';

  captureHead = (captureHead + 1) % CaptureSize;
  if(captureCount < CaptureSize) captureCount++;
}

void printHex(const char* packet){
  if(packet == nullptr) return;
  for(const char* cursor = packet; *cursor != '\0'; ++cursor){
    Serial.printf("%02X", static_cast<uint8_t>(*cursor));
    if(*(cursor + 1) != '\0') Serial.print(' ');
  }
}

void maybeLogPacket(bool tx, const char* packet, uint32_t nowMs){
  if(currentMode == UartDebugMode::Off || packet == nullptr) return;

  if(currentMode == UartDebugMode::Hex){
    Serial.printf("[UART][%s][HEX] ", directionName(tx));
    printHex(packet);
    Serial.println();
    return;
  }

  if(currentMode == UartDebugMode::Verbose){
    Serial.printf(
      "[UART][%s] t=%lu len=%u cmd=%s packet=%s\n",
      directionName(tx),
      (unsigned long)nowMs,
      (unsigned)strlen(packet),
      commandName(packet),
      packet
    );
    return;
  }

  Serial.printf("[UART][%s] %s\n", directionName(tx), commandName(packet));
}

}  // namespace

namespace UartTrafficMonitor {

void begin(){
  monitorStats = {};
  memset(capture, 0, sizeof(capture));
  captureHead = 0;
  captureCount = 0;
  dumpRate = {};
  currentMode = static_cast<UartDebugMode>(DESKDROID_UART_MONITOR_MODE);
}

void setMode(UartDebugMode mode){
  currentMode = mode;
}

UartDebugMode mode(){
  return currentMode;
}

void recordTx(const char* packet, uint32_t nowMs){
#if DESKDROID_UART_MONITOR_ENABLED
  if(packet == nullptr) return;
  monitorStats.txPackets++;
  monitorStats.txBytes += strlen(packet);
  store(true, packet, nowMs);
  maybeLogPacket(true, packet, nowMs);
#else
  (void)packet;
  (void)nowMs;
#endif
}

void recordRx(const char* packet, uint32_t nowMs){
#if DESKDROID_UART_MONITOR_ENABLED
  if(packet == nullptr) return;
  monitorStats.rxPackets++;
  monitorStats.rxBytes += strlen(packet);
  store(false, packet, nowMs);
  maybeLogPacket(false, packet, nowMs);
#else
  (void)packet;
  (void)nowMs;
#endif
}

void recordCrcError(uint32_t nowMs){
#if DESKDROID_UART_MONITOR_ENABLED
  monitorStats.crcErrors++;
#else
  (void)nowMs;
#endif
}

void recordTimeout(uint32_t nowMs){
#if DESKDROID_UART_MONITOR_ENABLED
  monitorStats.timeoutErrors++;
#else
  (void)nowMs;
#endif
}

void recordResync(uint32_t nowMs){
#if DESKDROID_UART_MONITOR_ENABLED
  monitorStats.resyncCount++;
#else
  (void)nowMs;
#endif
}

void recordMalformed(const char* reason, uint32_t nowMs){
#if DESKDROID_UART_MONITOR_ENABLED
  monitorStats.malformedPackets++;
  if(currentMode != UartDebugMode::Off){
    Serial.printf("[UART][ERR] malformed reason=%s t=%lu\n", reason, (unsigned long)nowMs);
  }
#else
  (void)reason;
  (void)nowMs;
#endif
}

void dumpRecent(const char* reason, uint32_t nowMs){
#if DESKDROID_UART_MONITOR_ENABLED
  if(currentMode == UartDebugMode::Off) return;
  if(!Log::shouldLog(dumpRate, DumpRateLimitMs, nowMs)) return;

  Serial.printf("[UART][DUMP] reason=%s count=%u\n", reason, captureCount);
  for(uint8_t i = 0; i < captureCount; ++i){
    const uint8_t index = (captureHead + CaptureSize - captureCount + i) % CaptureSize;
    const UartCaptureRecord &record = capture[index];
    Serial.printf(
      "[UART][DUMP][%s] t=%lu len=%u %s\n",
      directionName(record.tx),
      (unsigned long)record.timestampMs,
      record.length,
      record.packet
    );
  }
#else
  (void)reason;
  (void)nowMs;
#endif
}

const UartMonitorStats& stats(){
  return monitorStats;
}

}  // namespace UartTrafficMonitor
