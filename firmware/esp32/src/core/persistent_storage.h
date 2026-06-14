#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace PersistentStorage {

/// Initialize persistent storage (NVS namespace)
void begin();

/// Load relay states from persistent storage
/// Returns true if states were successfully loaded, false if no saved state exists
bool loadRelayStates(bool* states, uint8_t count);

/// Save relay states to persistent storage
/// Returns true if save was successful
bool saveRelayStates(const bool* states, uint8_t count);

}  // namespace PersistentStorage
