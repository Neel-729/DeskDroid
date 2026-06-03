#include "fault_tracker.h"

#include <string.h>

namespace {

constexpr uint8_t FaultRingSize = 12;
constexpr uint32_t FaultLogIntervalMs = 2000;

FaultRecord faultRing[FaultRingSize];
uint8_t faultHead = 0;
uint8_t faultCount = 0;
FaultSnapshot faultSnapshot;
RateLimitedLog faultLogRate;

void incrementCounter(FaultSource source){
  switch(source){
    case FaultSource::Link:
      faultSnapshot.counters.link++;
      break;
    case FaultSource::Protocol:
      faultSnapshot.counters.protocol++;
      break;
    case FaultSource::Scheduler:
      faultSnapshot.counters.schedulerOverrun++;
      break;
    case FaultSource::Hardware:
      faultSnapshot.counters.hardwareRequest++;
      break;
    case FaultSource::Driver:
      faultSnapshot.counters.driver++;
      break;
  }
}

void copyMessage(FaultRecord &record, const char* message){
  if(message == nullptr){
    record.message[0] = '\0';
    return;
  }
  strncpy(record.message, message, sizeof(record.message) - 1);
  record.message[sizeof(record.message) - 1] = '\0';
}

}  // namespace

namespace FaultTracker {

void begin(){
  memset(faultRing, 0, sizeof(faultRing));
  faultHead = 0;
  faultCount = 0;
  faultSnapshot = {};
  faultLogRate = {};
}

void record(FaultSource source, FaultCode code, const char* message, uint32_t nowMs){
  incrementCounter(source);

  FaultRecord &last = faultSnapshot.lastFault;
  if(last.code == code && last.source == source){
    if(last.repeatCount < 65535) last.repeatCount++;
    last.timestampMs = nowMs;
    copyMessage(last, message);
  } else {
    last.code = code;
    last.source = source;
    last.timestampMs = nowMs;
    last.repeatCount = 1;
    copyMessage(last, message);
  }

  faultRing[faultHead] = last;
  faultHead = (faultHead + 1) % FaultRingSize;
  if(faultCount < FaultRingSize) faultCount++;
  faultSnapshot.recentCount = faultCount;

#if DESKDROID_FAULT_LOGGING
  if(Log::shouldLog(faultLogRate, FaultLogIntervalMs, nowMs)){
    LOG_WARN(
      LogTag::SYSTEM,
      "fault src=%s code=%s repeat=%u msg=%s",
      sourceName(source),
      codeName(code),
      last.repeatCount,
      last.message
    );
  }
#else
  (void)nowMs;
#endif
}

const FaultSnapshot& snapshot(){
  return faultSnapshot;
}

const FaultRecord& lastFault(){
  return faultSnapshot.lastFault;
}

const char* codeName(FaultCode code){
  switch(code){
    case FaultCode::None: return "NONE";
    case FaultCode::LinkHeartbeatTimeout: return "LINK_HEARTBEAT_TIMEOUT";
    case FaultCode::LinkSyncFailure: return "LINK_SYNC_FAILURE";
    case FaultCode::LinkSerialFailure: return "LINK_SERIAL_FAILURE";
    case FaultCode::ProtocolMalformedPacket: return "PROTO_MALFORMED_PACKET";
    case FaultCode::ProtocolErrorPacket: return "PROTO_ERROR_PACKET";
    case FaultCode::SchedulerOverrun: return "SCHED_OVERRUN";
    case FaultCode::SchedulerInvalidTask: return "SCHED_INVALID_TASK";
    case FaultCode::HardwareQueueFull: return "HW_QUEUE_FULL";
    case FaultCode::DriverFailure: return "DRIVER_FAILURE";
  }
  return "UNKNOWN";
}

const char* sourceName(FaultSource source){
  switch(source){
    case FaultSource::Link: return "LINK";
    case FaultSource::Protocol: return "PROTO";
    case FaultSource::Scheduler: return "SCHED";
    case FaultSource::Hardware: return "HW";
    case FaultSource::Driver: return "DRIVER";
  }
  return "?";
}

}  // namespace FaultTracker
