#include "services.h"

#include "connectivity_service.h"
#include "lighting_service.h"
#include "protocol_service.h"
#include "timer_service.h"

namespace Services {

void begin(const DeviceSettings &settings){
  LightingService::begin(settings);
  ProtocolService::begin();
}

void update(uint32_t nowMs){
  (void)nowMs;
  ConnectivityService::update();
  ProtocolService::update();
}

void handleEvent(const AppEvent &event, uint32_t nowMs){
  LightingService::handleEvent(event);
  ProtocolService::handleEvent(event);
  TimerService::handleEvent(event, nowMs);
}

}  // namespace Services
