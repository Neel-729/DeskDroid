#include "connectivity_service.h"

#include <WiFi.h>

#include "../core/events.h"
#include "../core/system_state.h"

namespace {
bool lastWifiConnected = false;
}

namespace ConnectivityService {

void update(){
  const bool connected = WiFi.status() == WL_CONNECTED;
  const int32_t rssi = connected ? WiFi.RSSI() : 0;
  if(connected == lastWifiConnected && SystemStateStore::current().connectivity.rssi == rssi) return;

  lastWifiConnected = connected;
  SystemStateStore::setWifiConnected(connected, rssi);
  if(connected){
    enqueueEvent(EVENT_WIFI_CONNECTED, EventSource::SYSTEM);
  }
}

}  // namespace ConnectivityService
