#include "esp8266_link.h"

#include "config.h"
#include "pins.h"
#include "heartbeat_supervisor.h"
#include "packet_builder.h"
#include "parser.h"
#include "synchronization_manager.h"
#include "../core/logging.h"
#include "../core/system_state.h"

#include <string.h>

namespace {

constexpr char PacketStart = '<';
constexpr char PacketEnd = '>';

Stream* linkStream = nullptr;
HeartbeatSupervisor heartbeat;
SynchronizationManager synchronization;
Esp8266ConnectionState connectionState = Esp8266ConnectionState::Disconnected;
char rxPacket[Config::Esp8266MaxPacketSize + 1] = {};
size_t rxLength = 0;
bool assembling = false;
uint32_t observedStateRevision = 0;

const char* stateName(Esp8266ConnectionState state){
  switch(state){
    case Esp8266ConnectionState::Disconnected: return "DISCONNECTED";
    case Esp8266ConnectionState::Connecting: return "CONNECTING";
    case Esp8266ConnectionState::Syncing: return "SYNCING";
    case Esp8266ConnectionState::Running: return "RUNNING";
    case Esp8266ConnectionState::Error: return "ERROR";
  }
  return "UNKNOWN";
}

void transitionTo(Esp8266ConnectionState nextState){
  if(connectionState == nextState) return;
  connectionState = nextState;
  CONTROL_LOG_INFO(LogTag::LINK, "state=%s", stateName(nextState));
}

bool isAllowedPacketChar(char c){
  return c >= 32 && c <= 126;
}

void resetAssembler(){
  rxLength = 0;
  rxPacket[0] = '\0';
  assembling = false;
}

bool sendPacket(const char* packet){
  if(linkStream == nullptr || packet == nullptr || packet[0] == '\0') return false;
  linkStream->println(packet);
  return true;
}

bool sendPing(unsigned long now){
  char packet[Config::Esp8266MaxPacketSize + 1] = {};
  if(PacketBuilder::ping(packet, sizeof(packet)) == 0) return false;
  if(!sendPacket(packet)) return false;
  heartbeat.markPingSent(now);
  CONTROL_LOG_INFO(LogTag::HEARTBEAT, "ping");
  return true;
}

bool sendFullSync(unsigned long now){
  char packet[Config::Esp8266MaxPacketSize + 1] = {};
  if(PacketBuilder::fullSync(packet, sizeof(packet), SystemStateStore::current()) == 0){
    transitionTo(Esp8266ConnectionState::Error);
    return false;
  }

  if(!sendPacket(packet)) return false;

  synchronization.markSent(now);
  transitionTo(Esp8266ConnectionState::Syncing);
  CONTROL_LOG_INFO(LogTag::SYNC, "full sync sent rev=%lu", (unsigned long)SystemStateStore::revision());
  return true;
}

void requestSyncFromCurrentState(){
  synchronization.requestSync();
  observedStateRevision = SystemStateStore::revision();
}

void markStateDirtyIfChanged(){
  const uint32_t revision = SystemStateStore::revision();
  if(revision == observedStateRevision) return;
  observedStateRevision = revision;
  synchronization.requestSync();
  CONTROL_LOG_INFO(LogTag::STATE, "dirty rev=%lu", (unsigned long)revision);
}

void onAlivePacket(unsigned long now){
  heartbeat.recordPong(now);
  if(connectionState == Esp8266ConnectionState::Disconnected ||
     connectionState == Esp8266ConnectionState::Connecting ||
     connectionState == Esp8266ConnectionState::Error){
    synchronization.reset();
    sendFullSync(now);
  }
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
    sendFullSync(now);
  }
}

void handleParsedPacket(Esp32ProtocolParser::Packet &packet, unsigned long now){
  const char* command = packet.command();

  if(Esp32ProtocolParser::equals(command, "BOOT_READY")){
    heartbeat.recordPong(now);
    synchronization.reset();
    sendFullSync(now);
    return;
  }

  if(Esp32ProtocolParser::equals(command, "SYNC_OK")){
    heartbeat.recordPong(now);
    synchronization.complete();
    transitionTo(Esp8266ConnectionState::Running);
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

  if(Esp32ProtocolParser::equals(command, "ERR")){
    CONTROL_LOG_WARN(LogTag::PROTO, "execution plane error");
    if(connectionState == Esp8266ConnectionState::Syncing){
      transitionTo(Esp8266ConnectionState::Error);
      synchronization.reset();
    }
    return;
  }

  CONTROL_LOG_WARN(LogTag::PROTO, "unknown packet");
}

void processPacket(unsigned long now){
  char parseBuffer[Config::Esp8266MaxPacketSize + 1] = {};
  strncpy(parseBuffer, rxPacket, Config::Esp8266MaxPacketSize);
  parseBuffer[Config::Esp8266MaxPacketSize] = '\0';

  Esp32ProtocolParser::Packet packet;
  if(Esp32ProtocolParser::parse(parseBuffer, packet) != Esp32ProtocolParser::ParseResult::Ok){
    CONTROL_LOG_WARN(LogTag::PROTO, "malformed packet ignored");
    return;
  }

  handleParsedPacket(packet, now);
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
    transitionTo(Esp8266ConnectionState::Disconnected);
    synchronization.reset();
    CONTROL_LOG_WARN(LogTag::HEARTBEAT, "timeout");
  }

  switch(connectionState){
    case Esp8266ConnectionState::Disconnected:
    case Esp8266ConnectionState::Connecting:
    case Esp8266ConnectionState::Error:
      if(heartbeat.shouldSendPing(now)){
        sendPing(now);
      }
      break;

    case Esp8266ConnectionState::Syncing:
      if(synchronization.retryLimitReached()){
        transitionTo(Esp8266ConnectionState::Error);
        synchronization.reset();
        break;
      }
      if(synchronization.shouldSend(now)){
        sendFullSync(now);
      }
      break;

    case Esp8266ConnectionState::Running:
      if(synchronization.pending() && synchronization.shouldSend(now)){
        sendFullSync(now);
      } else if(heartbeat.shouldSendPing(now)){
        sendPing(now);
      }
      break;
  }
}

void markDirtyOnRevisionChange(uint32_t beforeRevision){
  if(SystemStateStore::revision() != beforeRevision){
    requestSyncFromCurrentState();
  }
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
  heartbeat.begin(millis());
  synchronization.begin();
  observedStateRevision = SystemStateStore::revision();
  transitionTo(Esp8266ConnectionState::Connecting);
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

bool setRelay(uint8_t relayNumber, bool enabled){
  const uint32_t before = SystemStateStore::revision();
  if(!SystemStateStore::setRelay(relayNumber, enabled)) return false;
  markDirtyOnRevisionChange(before);
  return true;
}

void setLedsEnabled(bool enabled){
  const uint32_t before = SystemStateStore::revision();
  SystemStateStore::setLedsEnabled(enabled);
  markDirtyOnRevisionChange(before);
}

void setBrightnessLevel(uint8_t level){
  const uint32_t before = SystemStateStore::revision();
  SystemStateStore::setBrightnessLevel(level);
  markDirtyOnRevisionChange(before);
}

void setIdlePreset(LedIdlePreset preset){
  const uint32_t before = SystemStateStore::revision();
  SystemStateStore::setIdlePreset(preset);
  markDirtyOnRevisionChange(before);
}

void setLedMode(LedState mode){
  const uint32_t before = SystemStateStore::revision();
  SystemStateStore::setLedMode(mode);
  markDirtyOnRevisionChange(before);
}

void requestFullSync(){
  requestSyncFromCurrentState();
}

}  // namespace Esp8266Link
