#include "relay_manager.h"

RelayManager::RelayManager(StateCache& state) : state_(state) {}

void RelayManager::begin() {
  for (uint8_t i = 0; i < Config::RelayCount; ++i) {
    const uint8_t pin = Pins::RelayPins[i];
    digitalWrite(pin, Config::RelayInactiveLevel);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, Config::RelayInactiveLevel);
    desiredStates_[i] = false;
    appliedStates_[i] = false;
    dirty_[i] = false;
  }
  allOff();
}

void RelayManager::update() {
  for (uint8_t i = 0; i < Config::RelayCount; ++i) {
    if (!dirty_[i] || desiredStates_[i] == appliedStates_[i]) {
      dirty_[i] = false;
      continue;
    }

    applyRelay(i, desiredStates_[i]);
    dirty_[i] = false;
  }
}

bool RelayManager::setRelay(uint8_t relayNumber, bool enabled) {
  if (relayNumber == 0 || relayNumber > Config::RelayCount) {
    return false;
  }

  const uint8_t index = relayNumber - 1;
  desiredStates_[index] = enabled;
  dirty_[index] = true;
  state_.setRelayState(relayNumber, enabled);
  return true;
}

void RelayManager::applyStateCache() {
  for (uint8_t relay = 1; relay <= Config::RelayCount; ++relay) {
    setRelay(relay, state_.relayState(relay));
  }
}

void RelayManager::allOff() {
  for (uint8_t i = 0; i < Config::RelayCount; ++i) {
    desiredStates_[i] = false;
    applyRelay(i, false);
    dirty_[i] = false;
    state_.setRelayState(i + 1, false);
  }
}

void RelayManager::applyRelay(uint8_t index, bool enabled) {
  if (index >= Config::RelayCount) {
    return;
  }

  digitalWrite(Pins::RelayPins[index], enabled ? Config::RelayActiveLevel : Config::RelayInactiveLevel);
  appliedStates_[index] = enabled;
}
