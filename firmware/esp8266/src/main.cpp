#include <Arduino.h>

#include "config.h"
#include "led/led_engine.h"
#include "protocol/protocol.h"
#include "relay/relay_manager.h"
#include "system/runtime.h"
#include "state/state_cache.h"
#include "system/heartbeat.h"
#include "system/watchdog.h"

StateCache stateCache;
RelayManager relayManager(stateCache);
LedEngine ledEngine(stateCache);
Heartbeat heartbeat;
Runtime runtime;
Watchdog watchdog;
Protocol protocol(Serial, relayManager, ledEngine);

void setup() {
  relayManager.begin();

  stateCache.begin();
  Serial.begin(Config::SerialBaud);
  protocol.begin();
  ledEngine.begin();
  heartbeat.begin();
  runtime.begin();
  watchdog.begin();

  Serial.println(F("<ACK|BOOT>"));
}

void loop() {
  protocol.update();
  relayManager.update();
  ledEngine.update();
  heartbeat.update();
  runtime.update();
  watchdog.update();
}
