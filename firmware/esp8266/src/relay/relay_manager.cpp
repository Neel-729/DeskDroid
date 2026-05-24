#include "relay_manager.h"

RelayManager::RelayManager(StateCache& state) : state_(state) {}

void RelayManager::begin() {
  for (uint8_t pin : Pins::RelayPins) {
    digitalWrite(pin, Config::RelayInactiveLevel);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, Config::RelayInactiveLevel);
  }
  allOff();
}

void RelayManager::update() {
  // Reserved for future relay timing/synchronization without changing loop shape.
}

bool RelayManager::setRelay(uint8_t relayNumber, bool enabled) {
  if (relayNumber == 0 || relayNumber > Config::RelayCount) {
    return false;
  }

  const uint8_t pin = Pins::RelayPins[relayNumber - 1];
  digitalWrite(pin, enabled ? Config::RelayActiveLevel : Config::RelayInactiveLevel);
  state_.setRelayState(relayNumber, enabled);
  return true;
}

void RelayManager::allOff() {
  for (uint8_t relay = 1; relay <= Config::RelayCount; ++relay) {
    setRelay(relay, false);
  }
}
