#pragma once

#include <Arduino.h>

#include "config.h"
#include "logging.h"

enum class FaultSource : uint8_t {
  Link,
  Protocol,
  Scheduler,
  Hardware,
  Driver,
};

enum class FaultCode : uint8_t {
  None,
  LinkHeartbeatTimeout,
  LinkSyncFailure,
  LinkSerialFailure,
  ProtocolMalformedPacket,
  ProtocolErrorPacket,
  SchedulerOverrun,
  SchedulerInvalidTask,
  HardwareQueueFull,
  DriverFailure,
};

struct FaultRecord {
  FaultCode code = FaultCode::None;
  FaultSource source = FaultSource::Link;
  uint32_t timestampMs = 0;
  uint16_t repeatCount = 0;
  char message[48] = {};
};

struct FaultCounters {
  uint32_t link = 0;
  uint32_t protocol = 0;
  uint32_t schedulerOverrun = 0;
  uint32_t hardwareRequest = 0;
  uint32_t driver = 0;
};

struct FaultSnapshot {
  FaultCounters counters;
  FaultRecord lastFault;
  uint8_t recentCount = 0;
};

namespace FaultTracker {

void begin();
void record(FaultSource source, FaultCode code, const char* message, uint32_t nowMs);
const FaultSnapshot& snapshot();
const FaultRecord& lastFault();
const char* codeName(FaultCode code);
const char* sourceName(FaultSource source);

}  // namespace FaultTracker

#if DESKDROID_FAULT_LOGGING
#define FAULT_DEBUG(tag, format, ...) \
  do { \
    Serial.printf("[%s][DBG] " format "\n", Log::tagName(tag), ##__VA_ARGS__); \
  } while(0)
#else
#define FAULT_DEBUG(tag, format, ...) do { } while(0)
#endif
