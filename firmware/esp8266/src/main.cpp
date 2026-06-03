#include <Arduino.h>
#include "config.h"
#include "led/led_engine.h"
#include "protocol/command_queue.h"
#include "protocol/packet_dispatcher.h"
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
Runtime runtime(Serial);
Watchdog watchdog;
PacketDispatcher packetDispatcher(Serial, stateCache, relayManager, ledEngine, runtime);
CommandQueue commandQueue(packetDispatcher);
Protocol protocol(Serial, commandQueue);

void setup() {
  relayManager.begin();

  stateCache.begin();
  Serial.begin(Config::SerialBaud);
  protocol.begin();
  commandQueue.begin();
  ledEngine.begin();
  heartbeat.begin();
  runtime.begin();
  watchdog.begin(runtime, protocol, commandQueue, Serial);

  Serial.println(F("<BOOT_READY>"));
  runtime.markBootReadySent();
}

void loop() {
  protocol.update();
  heartbeat.update();
  commandQueue.update();
  relayManager.update();
  ledEngine.update();
  runtime.update();
  watchdog.update();
  yield();
}
