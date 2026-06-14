#include "esp8266_link.h"

#include "config.h"
#include "pins.h"
#include "heartbeat_supervisor.h"
#include "packet_builder.h"
#include "parser.h"
#include "synchronization_manager.h"
#include "uart_monitor.h"
#include "../core/fault_tracker.h"
#include "../core/logging.h"
#include "../core/system_state.h"

#include <string.h>

namespace {

constexpr char PacketStart = '<';
constexpr char PacketEnd = '>';
constexpr uint8_t DefaultLedSpeed = 5;
constexpr uint32_t LedAckTimeoutMs = 750;
constexpr uint32_t LedRetryBackoffBaseMs = 1200;
constexpr uint32_t LedRetryBackoffMaxMs = 30000;
constexpr uint8_t MaxLedRetries = 3;

Stream* linkStream = nullptr;
HeartbeatSupervisor heartbeat;
SynchronizationManager synchronization;
Esp8266ConnectionState connectionState = Esp8266ConnectionState::Disconnected;
Esp8266LinkDiagnostics linkDiagnostics;
char rxPacket[Config::Esp8266MaxPacketSize + 1] = {};
size_t rxLength = 0;
bool assembling = false;
uint32_t observedStateRevision = 0;
uint16_t nextTxSequenceId = 1;
uint16_t lastSyncSequenceId = 0;
uint8_t recoveryAttemptCount = 0;
uint16_t transportResetCount = 0;
uint32_t recoveryDueMs = 0;
uint32_t lastRecoveryMs = 0;
const char* lastRecoveryReason = "";
const char* lastSyncReason = "";
uint8_t malformedBurstCount = 0;
uint32_t malformedBurstStartMs = 0;
char lastLedErrReason[32] = "";

struct LedWireState {
  bool power = false;
  uint8_t brightness = 0;
  uint8_t speed = DefaultLedSpeed;
  AnimationMode mode = AnimationMode::None;
  RGBColor color;
};

LedWireState lastSentLedState;
LedWireState appliedLedState;
LedWireState pendingLedState;
bool hasLastSentLedState = false;
bool hasAppliedLedState = false;
bool hasPendingLedState = false;
bool ledResyncRequired = true;
LedApplyStatus ledStatus = LedApplyStatus::Idle;
uint8_t ledRetryCount = 0;
uint16_t pendingLedSequenceId = 0;
uint32_t lastLedTxMs = 0;
uint32_t lastLedAckMs = 0;
uint32_t nextLedRetryDueMs = 0;
uint32_t ledTxCount = 0;
uint32_t ledAckCount = 0;
uint32_t ledRetryCountTotal = 0;
uint32_t ledDuplicateIgnoredCount = 0;

bool sameColor(const RGBColor &left, const RGBColor &right){
  return left.r == right.r && left.g == right.g && left.b == right.b;
}

bool sameLedState(const LedWireState &left, const LedWireState &right){
  return left.power == right.power &&
    left.brightness == right.brightness &&
    left.speed == right.speed &&
    left.mode == right.mode &&
    sameColor(left.color, right.color);
}

uint32_t ledRetryBackoffMs(){
  uint32_t backoff = LedRetryBackoffBaseMs;
  uint8_t shifts = ledRetryCount == 0 ? 0 : ledRetryCount - 1;
  if(shifts > 5) shifts = 5;
  while(shifts-- > 0 && backoff < LedRetryBackoffMaxMs / 2){
    backoff *= 2;
  }
  return backoff > LedRetryBackoffMaxMs ? LedRetryBackoffMaxMs : backoff;
}

void setLedStatus(LedApplyStatus nextStatus){
  ledStatus = nextStatus;
}

LedWireState desiredLedState(){
  const LightingState &lighting = SystemStateStore::current().lighting;
  LedWireState state;
  state.power = lighting.enabled && lighting.scheduleAllowsOutput;
  state.brightness = lighting.brightness;
  state.speed = DefaultLedSpeed;
  state.mode = lighting.mode;
  state.color = lighting.color;
  return state;
}

void copyText(char* destination, size_t destinationSize, const char* source){
  if(destination == nullptr || destinationSize == 0) return;
  if(source == nullptr) source = "";
  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

void transitionTo(Esp8266ConnectionState nextState){
  if(connectionState == nextState) return;
  connectionState = nextState;
  CONTROL_LOG_INFO(LogTag::LINK, "state=%s", Esp8266Link::stateName(nextState));
}

bool isAllowedPacketChar(char c){
  return c >= 32 && c <= 126;
}

void resetAssembler(){
  rxLength = 0;
  rxPacket[0] = '\0';
  assembling = false;
}

void resetMalformedBurst(){
  malformedBurstCount = 0;
  malformedBurstStartMs = 0;
}

void framedRxPacket(char* destination, size_t destinationSize){
  if(destination == nullptr || destinationSize == 0) return;
  snprintf(destination, destinationSize, "<%s>", rxPacket);
}

void refreshDiagnostics(){
  const LedWireState desired = desiredLedState();
  linkDiagnostics.state = connectionState;
  linkDiagnostics.retryCount = synchronization.retries();
  linkDiagnostics.recoveryAttemptCount = recoveryAttemptCount;
  linkDiagnostics.transportResetCount = transportResetCount;
  linkDiagnostics.lastRecoveryMs = lastRecoveryMs;
  linkDiagnostics.lastRecoveryReason = lastRecoveryReason;
  linkDiagnostics.lastSyncReason = lastSyncReason;
  linkDiagnostics.desiredLedMode = PacketBuilder::effectName(desired.mode);
  linkDiagnostics.desiredLedPower = desired.power;
  linkDiagnostics.desiredLedBrightness = desired.brightness;
  linkDiagnostics.desiredLedSpeed = desired.speed;
  linkDiagnostics.activeLedMode =
    hasAppliedLedState ? PacketBuilder::effectName(appliedLedState.mode) : "NONE";
  linkDiagnostics.activeLedPower = hasAppliedLedState ? appliedLedState.power : false;
  linkDiagnostics.activeLedBrightness = hasAppliedLedState ? appliedLedState.brightness : 0;
  linkDiagnostics.activeLedSpeed = hasAppliedLedState ? appliedLedState.speed : 0;
  linkDiagnostics.lastSentLedMode =
    hasLastSentLedState ? PacketBuilder::effectName(lastSentLedState.mode) : "NONE";
  linkDiagnostics.lastSentLedPower = hasLastSentLedState ? lastSentLedState.power : false;
  linkDiagnostics.lastSentLedBrightness = hasLastSentLedState ? lastSentLedState.brightness : 0;
  linkDiagnostics.lastSentLedSpeed = hasLastSentLedState ? lastSentLedState.speed : 0;
  linkDiagnostics.pendingLedMode =
    hasPendingLedState ? PacketBuilder::effectName(pendingLedState.mode) : "NONE";
  linkDiagnostics.pendingLedPower = hasPendingLedState ? pendingLedState.power : false;
  linkDiagnostics.pendingLedBrightness = hasPendingLedState ? pendingLedState.brightness : 0;
  linkDiagnostics.pendingLedSpeed = hasPendingLedState ? pendingLedState.speed : 0;
  linkDiagnostics.ledStatus = ledStatus;
  linkDiagnostics.ledRetryCount = ledRetryCount;
  linkDiagnostics.lastLedAckMs = lastLedAckMs;
  linkDiagnostics.lastLedErrReason = lastLedErrReason;
  linkDiagnostics.ledTx = ledTxCount;
  linkDiagnostics.ledAck = ledAckCount;
  linkDiagnostics.ledRetry = ledRetryCountTotal;
  linkDiagnostics.ledDuplicateIgnored = ledDuplicateIgnoredCount;
}

void reinitializeTransport(unsigned long now){
  if(linkStream != nullptr){
    Serial2.flush();
    Serial2.end();
  }
  Serial2.begin(
    Config::Esp8266SerialBaud,
    SERIAL_8N1,
    Pins::Esp8266UartRx,
    Pins::Esp8266UartTx
  );
  linkStream = &Serial2;
  heartbeat.begin(now);
  synchronization.begin();
  resetAssembler();
  resetMalformedBurst();
  transportResetCount++;
  UartTrafficMonitor::recordResync(now);
  CONTROL_LOG_WARN(LogTag::LINK, "transport reset count=%u", transportResetCount);
}

void enterRecovery(unsigned long now, FaultSource source, FaultCode code, const char* reason){
  FaultTracker::record(source, code, reason, now);
  UartTrafficMonitor::dumpRecent(reason, now);
  resetAssembler();
  synchronization.reset();
  lastRecoveryReason = reason;
  lastRecoveryMs = now;
  if(recoveryAttemptCount < 255) recoveryAttemptCount++;
  recoveryDueMs = now + Config::Esp8266RecoveryBackoffMs;
  UartTrafficMonitor::recordResync(now);
  transitionTo(Esp8266ConnectionState::Recovering);

  if(recoveryAttemptCount >= Config::Esp8266TransportResetThreshold){
    reinitializeTransport(now);
    recoveryAttemptCount = 0;
  }
  refreshDiagnostics();
}

bool sendPacket(const char* packet, unsigned long now){
  if(linkStream == nullptr || packet == nullptr || packet[0] == '\0') return false;
  linkStream->println(packet);
  UartTrafficMonitor::recordTx(packet, now);
  return true;
}

uint16_t nextSequenceId(){
  const uint16_t sequenceId = nextTxSequenceId++;
  if(nextTxSequenceId == 0) nextTxSequenceId = 1;
  return sequenceId;
}

bool sendPing(unsigned long now){
  char packet[Config::Esp8266MaxPacketSize + 1] = {};
  if(PacketBuilder::ping(packet, sizeof(packet), nextSequenceId()) == 0) return false;
  if(!sendPacket(packet, now)) return false;
  heartbeat.markPingSent(now);
  CONTROL_LOG_INFO(LogTag::HEARTBEAT, "ping");
  return true;
}

bool sendFullSync(unsigned long now, const char* reason){
  char packet[Config::Esp8266MaxPacketSize + 1] = {};
  lastSyncSequenceId = nextSequenceId();
  lastSyncReason = reason != nullptr ? reason : "";
  if(PacketBuilder::fullSync(packet, sizeof(packet), SystemStateStore::current(), lastSyncSequenceId) == 0){
    enterRecovery(now, FaultSource::Link, FaultCode::LinkSyncFailure, "sync_build_failed");
    return false;
  }

  if(!sendPacket(packet, now)) return false;

  synchronization.markSent(now);
  transitionTo(Esp8266ConnectionState::Syncing);
  CONTROL_LOG_INFO(LogTag::SYNC, "full sync sent rev=%lu reason=%s", (unsigned long)SystemStateStore::revision(), lastSyncReason);
  return true;
}

bool sendLedState(const LedWireState &state, unsigned long now){
  char packet[Config::Esp8266MaxPacketSize + 1] = {};
  pendingLedSequenceId = nextSequenceId();

  LightingState lighting = SystemStateStore::current().lighting;
  lighting.enabled = state.power;
  lighting.scheduleAllowsOutput = true;
  lighting.brightness = state.brightness;
  lighting.mode = state.mode;
  lighting.color = state.color;

  if(PacketBuilder::setLedState(packet, sizeof(packet), lighting, state.speed, pendingLedSequenceId) == 0){
    copyText(lastLedErrReason, sizeof(lastLedErrReason), "led_packet_build_failed");
    setLedStatus(LedApplyStatus::Error);
    LOG_WARN(LogTag::LED, "[LED_SYNC] ack_failure reason=%s", lastLedErrReason);
    return false;
  }

  if(!sendPacket(packet, now)){
    copyText(lastLedErrReason, sizeof(lastLedErrReason), "led_send_failed");
    setLedStatus(LedApplyStatus::Error);
    LOG_WARN(LogTag::LED, "[LED_SYNC] ack_failure reason=%s", lastLedErrReason);
    return false;
  }

  pendingLedState = state;
  lastSentLedState = state;
  hasPendingLedState = true;
  hasLastSentLedState = true;
  setLedStatus(LedApplyStatus::WaitingAck);
  lastLedTxMs = now;
  nextLedRetryDueMs = now + LedAckTimeoutMs;
  ledTxCount++;
  LOG_INFO(
    LogTag::LED,
    "[LED_SYNC] tx mode=%s brightness=%u speed=%u power=%u retry=%u",
    PacketBuilder::effectName(state.mode),
    state.brightness,
    state.speed,
    state.power ? 1 : 0,
    ledRetryCount
  );
  return true;
}

void markLedResyncRequired(){
  ledResyncRequired = true;
}

void queueLedState(const LedWireState &state, unsigned long now, bool resetRetries){
  if(hasPendingLedState && sameLedState(state, pendingLedState) && ledStatus == LedApplyStatus::PendingTx){
    return;
  }

  pendingLedState = state;
  hasPendingLedState = true;
  pendingLedSequenceId = 0;
  nextLedRetryDueMs = 0;
  if(resetRetries) ledRetryCount = 0;
  setLedStatus(LedApplyStatus::PendingTx);
  LOG_INFO(LogTag::LED, "[LED_SYNC] desired=%s", PacketBuilder::effectName(state.mode));
  (void)now;
}

void scheduleLedRetry(unsigned long now, const char* reason){
  if(ledRetryCount >= MaxLedRetries){
    hasPendingLedState = false;
    pendingLedSequenceId = 0;
    setLedStatus(LedApplyStatus::Error);
    LOG_WARN(LogTag::LED, "[LED_SYNC] ack_failure reason=%s retries_exhausted=%u", reason, ledRetryCount);
    return;
  }

  if(ledRetryCount < 255) ledRetryCount++;
  ledRetryCountTotal++;
  nextLedRetryDueMs = now + ledRetryBackoffMs();
  setLedStatus(LedApplyStatus::PendingTx);
  LOG_WARN(LogTag::LED, "[LED_SYNC] retry=%u reason=%s", ledRetryCount, reason);
}

void serviceLedState(unsigned long now){
  if(connectionState != Esp8266ConnectionState::Running) return;

  const LedWireState desired = desiredLedState();
  if(ledResyncRequired){
    ledResyncRequired = false;
    queueLedState(desired, now, true);
  }

  if(ledStatus == LedApplyStatus::Idle || ledStatus == LedApplyStatus::Synced){
    if(hasAppliedLedState && sameLedState(desired, appliedLedState)){
      return;
    }
    queueLedState(desired, now, true);
  }

  if(ledStatus == LedApplyStatus::WaitingAck){
    if(hasPendingLedState && !sameLedState(desired, pendingLedState)){
      queueLedState(desired, now, true);
      return;
    }
    if(nextLedRetryDueMs != 0 && now >= nextLedRetryDueMs){
      LOG_WARN(LogTag::LED, "[LED_SYNC] timeout seq=%u", pendingLedSequenceId);
      scheduleLedRetry(now, "timeout");
    }
    return;
  }

  if(ledStatus == LedApplyStatus::PendingTx){
    if(!hasPendingLedState){
      queueLedState(desired, now, true);
    }
    if(nextLedRetryDueMs != 0 && now < nextLedRetryDueMs){
      return;
    }
    sendLedState(pendingLedState, now);
  }
}

void requestSyncFromCurrentState(){
  synchronization.requestSync();
  observedStateRevision = SystemStateStore::revision();
}

void markStateDirtyIfChanged(){
  const uint32_t revision = SystemStateStore::revision();
  if(revision == observedStateRevision) return;
  observedStateRevision = revision;
  const uint16_t mask = SystemStateStore::lastChangeMask();
  if((mask & static_cast<uint16_t>(StateChange::Lighting)) != 0){
    const LedWireState desired = desiredLedState();
    const bool desiredChanged =
      !hasAppliedLedState ||
      !sameLedState(desired, appliedLedState) ||
      (hasPendingLedState && !sameLedState(desired, pendingLedState)) ||
      ledStatus == LedApplyStatus::Error;
    if(desiredChanged){
      queueLedState(desired, millis(), true);
    }
  }
  if((mask & static_cast<uint16_t>(StateChange::Protocol)) != 0){
    synchronization.requestSync();
  }
  CONTROL_LOG_INFO(LogTag::STATE, "dirty rev=%lu", (unsigned long)revision);
}

void onAlivePacket(unsigned long now){
  heartbeat.recordPong(now);
  if(connectionState == Esp8266ConnectionState::Disconnected ||
     connectionState == Esp8266ConnectionState::Connecting ||
    connectionState == Esp8266ConnectionState::Recovering ||
     connectionState == Esp8266ConnectionState::Error){
    synchronization.reset();
    sendFullSync(now, "alive_packet");
  }
}

const char* tokenValue(const Esp32ProtocolParser::Packet &packet, const char* key){
  const size_t keyLength = strlen(key);
  for(uint8_t i = 1; i < packet.tokenCount; ++i){
    if(strncmp(packet.tokens[i], key, keyLength) == 0 && packet.tokens[i][keyLength] == '='){
      return packet.tokens[i] + keyLength + 1;
    }
  }
  return nullptr;
}

uint16_t parseSequenceValue(const char* value){
  if(value == nullptr || value[0] == '\0') return 0;
  uint32_t parsed = 0;
  for(const char* cursor = value; *cursor != '\0'; ++cursor){
    if(*cursor < '0' || *cursor > '9') return 0;
    parsed = (parsed * 10UL) + (uint32_t)(*cursor - '0');
    if(parsed > 65535UL) return 0;
  }
  return (uint16_t)parsed;
}

void handleLedAck(const Esp32ProtocolParser::Packet &packet, unsigned long now){
  heartbeat.recordPong(now);
  const uint16_t sequence = parseSequenceValue(tokenValue(packet, "SEQ"));
  if(!hasPendingLedState || sequence != pendingLedSequenceId){
    LOG_WARN(LogTag::LED, "[LED_SYNC] stale_ack seq=%u pending=%u", sequence, pendingLedSequenceId);
    return;
  }

  appliedLedState = pendingLedState;
  hasAppliedLedState = true;
  hasPendingLedState = false;
  pendingLedSequenceId = 0;
  nextLedRetryDueMs = 0;
  ledResyncRequired = false;
  setLedStatus(LedApplyStatus::Synced);
  ledRetryCount = 0;
  lastLedAckMs = now;
  ledAckCount++;
  if(tokenValue(packet, "DUP") != nullptr){
    ledDuplicateIgnoredCount++;
  }
  copyText(lastLedErrReason, sizeof(lastLedErrReason), "");
  LOG_INFO(LogTag::LED, "[LED_SYNC] ack_success mode=%s", PacketBuilder::effectName(appliedLedState.mode));
  LOG_INFO(LogTag::LED, "[LED_SYNC] synced");
}

void handleLedError(const Esp32ProtocolParser::Packet &packet, unsigned long now){
  heartbeat.recordPong(now);
  const uint16_t sequence = parseSequenceValue(tokenValue(packet, "SEQ"));
  const char* reason = tokenValue(packet, "REASON");
  copyText(lastLedErrReason, sizeof(lastLedErrReason), reason != nullptr ? reason : "UNKNOWN");
  if(!hasPendingLedState || sequence != pendingLedSequenceId){
    LOG_WARN(LogTag::LED, "[LED_SYNC] stale_failure seq=%u pending=%u reason=%s", sequence, pendingLedSequenceId, lastLedErrReason);
    return;
  }

  pendingLedSequenceId = 0;
  lastLedTxMs = now;
  LOG_WARN(LogTag::LED, "[LED_SYNC] ack_failure reason=%s", lastLedErrReason);
  scheduleLedRetry(now, lastLedErrReason);
}

void handleAck(const Esp32ProtocolParser::Packet &packet, unsigned long now){
  if(packet.tokenCount < 2){
    return;
  }

  heartbeat.recordPong(now);
  if(Esp32ProtocolParser::equals(packet.tokens[1], "PING")){
    onAlivePacket(now);
  } else if(connectionState != Esp8266ConnectionState::Running){
    synchronization.reset();
    sendFullSync(now, "ack_before_running");
  }
}

void handleParsedPacket(Esp32ProtocolParser::Packet &packet, unsigned long now){
  const char* command = packet.command();

  if(Esp32ProtocolParser::equals(command, "BOOT_READY")){
    heartbeat.recordPong(now);
    synchronization.reset();
    sendFullSync(now, "boot_ready");
    return;
  }

  if(Esp32ProtocolParser::equals(command, "SYNC_OK")){
    heartbeat.recordPong(now);
    synchronization.complete();
    SystemStateStore::markProtocolSynced(SystemStateStore::revision(), lastSyncSequenceId);
    observedStateRevision = SystemStateStore::revision();
    recoveryAttemptCount = 0;
    resetMalformedBurst();
    transitionTo(Esp8266ConnectionState::Running);
    markLedResyncRequired();
    CONTROL_LOG_INFO(LogTag::SYNC, "sync ok");
    return;
  }

  if(Esp32ProtocolParser::equals(command, "PONG")){
    onAlivePacket(now);
    return;
  }

  if(Esp32ProtocolParser::equals(command, "ACK")){
    handleAck(packet, now);
    return;
  }

  if(Esp32ProtocolParser::equals(command, "ACK_LED_STATE") ||
     Esp32ProtocolParser::equals(command, "ACK_SUCCESS")){
    handleLedAck(packet, now);
    return;
  }

  if(Esp32ProtocolParser::equals(command, "ERR_LED_STATE") ||
     Esp32ProtocolParser::equals(command, "ACK_FAILURE")){
    handleLedError(packet, now);
    return;
  }

  if(Esp32ProtocolParser::equals(command, "ERR")){
    CONTROL_LOG_WARN(LogTag::PROTO, "execution plane error");
    enterRecovery(now, FaultSource::Protocol, FaultCode::ProtocolErrorPacket, "esp8266_error_packet");
    if(connectionState == Esp8266ConnectionState::Syncing){
      transitionTo(Esp8266ConnectionState::Error);
      synchronization.reset();
    }
    return;
  }

  CONTROL_LOG_WARN(LogTag::PROTO, "unknown packet");
}

void processPacket(unsigned long now){
  char framedPacket[Config::Esp8266MaxPacketSize + 3] = {};
  framedRxPacket(framedPacket, sizeof(framedPacket));
  UartTrafficMonitor::recordRx(framedPacket, now);

  char parseBuffer[Config::Esp8266MaxPacketSize + 1] = {};
  strncpy(parseBuffer, rxPacket, Config::Esp8266MaxPacketSize);
  parseBuffer[Config::Esp8266MaxPacketSize] = '\0';

  Esp32ProtocolParser::Packet packet;
  if(Esp32ProtocolParser::parse(parseBuffer, packet) != Esp32ProtocolParser::ParseResult::Ok){
    UartTrafficMonitor::recordMalformed("parse_failed", now);
    FaultTracker::record(FaultSource::Protocol, FaultCode::ProtocolMalformedPacket, "parse_failed", now);
    CONTROL_LOG_WARN(LogTag::PROTO, "malformed packet ignored");
    return;
  }

  resetMalformedBurst();
  handleParsedPacket(packet, now);
}

void recordMalformedPacket(unsigned long now, const char* reason){
  UartTrafficMonitor::recordMalformed(reason, now);
  FaultTracker::record(FaultSource::Protocol, FaultCode::ProtocolMalformedPacket, reason, now);

  if(malformedBurstStartMs == 0 || now - malformedBurstStartMs > Config::Esp8266MalformedBurstWindowMs){
    malformedBurstStartMs = now;
    malformedBurstCount = 0;
  }
  if(malformedBurstCount < 255) malformedBurstCount++;

  if(malformedBurstCount >= Config::Esp8266MalformedBurstLimit){
    resetMalformedBurst();
    enterRecovery(now, FaultSource::Protocol, FaultCode::ProtocolMalformedPacket, "malformed_burst");
  }
}

void consume(char c, unsigned long now){
  if(c == PacketStart){
    rxLength = 0;
    rxPacket[0] = '\0';
    assembling = true;
    return;
  }

  if(!assembling){
    return;
  }

  if(c == PacketEnd){
    rxPacket[rxLength] = '\0';
    assembling = false;
    processPacket(now);
    return;
  }

  if(c == '\r' || c == '\n'){
    return;
  }

  if(rxLength >= Config::Esp8266MaxPacketSize || !isAllowedPacketChar(c)){
    resetAssembler();
    recordMalformedPacket(now, "assembler_rejected");
    CONTROL_LOG_WARN(LogTag::PROTO, "packet rejected");
    return;
  }

  rxPacket[rxLength++] = c;
  rxPacket[rxLength] = '\0';
}

void receive(unsigned long now){
  if(linkStream == nullptr) return;

  uint8_t processed = 0;
  while(linkStream->available() > 0 && processed < Config::Esp8266MaxSerialBytesPerUpdate){
    consume(static_cast<char>(linkStream->read()), now);
    processed++;
  }
}

void supervise(unsigned long now){
  markStateDirtyIfChanged();

  if(connectionState == Esp8266ConnectionState::Running && heartbeat.timedOut(now)){
    UartTrafficMonitor::recordTimeout(now);
    enterRecovery(now, FaultSource::Link, FaultCode::LinkHeartbeatTimeout, "heartbeat_timeout");
    CONTROL_LOG_WARN(LogTag::HEARTBEAT, "timeout");
  }

  switch(connectionState){
    case Esp8266ConnectionState::Disconnected:
    case Esp8266ConnectionState::Connecting:
    case Esp8266ConnectionState::Error:
      if(heartbeat.shouldSendPing(now)){
        if(!sendPing(now)){
          enterRecovery(now, FaultSource::Link, FaultCode::LinkSerialFailure, "ping_send_failed");
        }
      }
      break;

    case Esp8266ConnectionState::Syncing:
      if(synchronization.retryLimitReached()){
        enterRecovery(now, FaultSource::Link, FaultCode::LinkSyncFailure, "sync_retry_limit");
        break;
      }
      if(synchronization.shouldSend(now)){
        if(!sendFullSync(now, "sync_retry")){
          enterRecovery(now, FaultSource::Link, FaultCode::LinkSerialFailure, "sync_send_failed");
        }
      }
      break;

    case Esp8266ConnectionState::Running:
      if(synchronization.pending() && synchronization.shouldSend(now)){
        if(!sendFullSync(now, "state_pending")){
          enterRecovery(now, FaultSource::Link, FaultCode::LinkSerialFailure, "sync_send_failed");
        }
      } else if(heartbeat.shouldSendPing(now)){
        if(!sendPing(now)){
          enterRecovery(now, FaultSource::Link, FaultCode::LinkSerialFailure, "ping_send_failed");
        }
      }
      break;

    case Esp8266ConnectionState::Recovering:
      if(now >= recoveryDueMs){
        if(!sendFullSync(now, "recovery")){
          enterRecovery(now, FaultSource::Link, FaultCode::LinkSerialFailure, "recovery_sync_failed");
        }
      }
      break;
  }

  serviceLedState(now);
  refreshDiagnostics();
}

}  // namespace

namespace Esp8266Link {

void begin() {
  Serial2.begin(
    Config::Esp8266SerialBaud,
    SERIAL_8N1,
    Pins::Esp8266UartRx,
    Pins::Esp8266UartTx
  );
  linkStream = &Serial2;
  UartTrafficMonitor::begin();
  heartbeat.begin(millis());
  synchronization.begin();
  observedStateRevision = SystemStateStore::revision();
  recoveryAttemptCount = 0;
  transportResetCount = 0;
  recoveryDueMs = 0;
  lastRecoveryMs = 0;
  lastRecoveryReason = "";
  lastSyncReason = "";
  markLedResyncRequired();
  hasLastSentLedState = false;
  hasAppliedLedState = false;
  ledStatus = LedApplyStatus::Idle;
  ledRetryCount = 0;
  lastLedAckMs = 0;
  nextLedRetryDueMs = 0;
  ledTxCount = 0;
  ledAckCount = 0;
  ledRetryCountTotal = 0;
  ledDuplicateIgnoredCount = 0;
  copyText(lastLedErrReason, sizeof(lastLedErrReason), "");
  resetAssembler();
  resetMalformedBurst();
  transitionTo(Esp8266ConnectionState::Connecting);
  refreshDiagnostics();
}

void update() {
  const unsigned long now = millis();
  receive(now);
  supervise(now);
}

bool isLinked() {
  return connectionState == Esp8266ConnectionState::Running;
}

bool isRunning() {
  return connectionState == Esp8266ConnectionState::Running;
}

Esp8266ConnectionState state() {
  return connectionState;
}

void requestFullSync(){
  requestSyncFromCurrentState();
}

const Esp8266LinkDiagnostics& diagnostics(){
  refreshDiagnostics();
  return linkDiagnostics;
}

const char* stateName(Esp8266ConnectionState state){
  switch(state){
    case Esp8266ConnectionState::Disconnected: return "DISCONNECTED";
    case Esp8266ConnectionState::Connecting: return "CONNECTING";
    case Esp8266ConnectionState::Syncing: return "SYNCING";
    case Esp8266ConnectionState::Running: return "RUNNING";
    case Esp8266ConnectionState::Recovering: return "RECOVERING";
    case Esp8266ConnectionState::Error: return "ERROR";
  }
  return "UNKNOWN";
}

const char* ledStatusName(LedApplyStatus status){
  switch(status){
    case LedApplyStatus::Idle: return "IDLE";
    case LedApplyStatus::PendingTx: return "PENDING_TX";
    case LedApplyStatus::WaitingAck: return "WAITING_ACK";
    case LedApplyStatus::Synced: return "SYNCED";
    case LedApplyStatus::Error: return "ERROR";
  }
  return "UNKNOWN";
}

}  // namespace Esp8266Link
