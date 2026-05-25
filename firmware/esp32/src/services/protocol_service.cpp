#include "protocol_service.h"

#include "../core/system_state.h"
#include "../protocol/esp8266_link.h"

namespace {
bool lastConnected = false;
}

namespace ProtocolService {

void begin(){
  Esp8266Link::begin();
  lastConnected = Esp8266Link::isRunning();
  SystemStateStore::setEsp8266Connected(lastConnected);
}

void update(){
  Esp8266Link::update();

  const bool nowConnected = Esp8266Link::isRunning();
  if(nowConnected == lastConnected) return;

  lastConnected = nowConnected;
  SystemStateStore::setEsp8266Connected(nowConnected);
  enqueueEvent(
    nowConnected ? EVENT_PROTOCOL_CONNECTED : EVENT_PROTOCOL_DISCONNECTED,
    EventSource::SYSTEM
  );
}

void handleEvent(const AppEvent &event){
  if(event.type != EVENT_STATE_CHANGED) return;
  const uint16_t mask = event.payload.state.changeMask;
  const uint16_t lightingMask = static_cast<uint16_t>(StateChange::Lighting);
  const uint16_t protocolMask = static_cast<uint16_t>(StateChange::Protocol);
  if((mask & (lightingMask | protocolMask)) == 0) return;
  if((mask & protocolMask) != 0 &&
     SystemStateStore::current().protocol.lastSyncRevision == SystemStateStore::revision()){
    return;
  }
  requestStateSync();
}

void requestStateSync(){
  Esp8266Link::requestFullSync();
}

bool connected(){
  return Esp8266Link::isRunning();
}

}  // namespace ProtocolService
