#include "persistent_storage.h"

#include "logging.h"

namespace {

Preferences preferences;
constexpr const char* NAMESPACE_NAME = "DeskDroid";
constexpr const char* RELAY_KEY = "relays";

}  // namespace

namespace PersistentStorage {

void begin() {
  preferences.begin(NAMESPACE_NAME, false);  // false = read/write mode
}

bool loadRelayStates(bool* states, uint8_t count) {
  if (!states || count == 0 || count > 8) {
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] loadRelayStates: invalid parameters");
    return false;
  }

  // Check if the relay key exists
  if (!preferences.isKey(RELAY_KEY)) {
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] loadRelayStates: no saved state found");
    return false;
  }

  try {
    // Read the packed relay state (stored as a single byte)
    uint8_t packed = preferences.getUChar(RELAY_KEY, 0);
    
    for (uint8_t i = 0; i < count; i++) {
      states[i] = (packed >> i) & 0x01;
    }
    
    // Log the loaded states
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] Loaded: R1=%s R2=%s R3=%s R4=%s",
        states[0] ? "ON" : "OFF",
        count > 1 ? (states[1] ? "ON" : "OFF") : "N/A",
        count > 2 ? (states[2] ? "ON" : "OFF") : "N/A",
        count > 3 ? (states[3] ? "ON" : "OFF") : "N/A");
    
    return true;
  } catch (...) {
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] loadRelayStates: exception reading preferences");
    return false;
  }
}

bool saveRelayStates(const bool* states, uint8_t count) {
  if (!states || count == 0 || count > 8) {
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] saveRelayStates: invalid parameters");
    return false;
  }

  try {
    // Pack relay states into a single byte
    uint8_t packed = 0;
    for (uint8_t i = 0; i < count; i++) {
      if (states[i]) {
        packed |= (1 << i);
      }
    }

    // Write to persistent storage
    preferences.putUChar(RELAY_KEY, packed);

    // Log the saved states
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] Saved: R1=%s R2=%s R3=%s R4=%s",
        states[0] ? "ON" : "OFF",
        count > 1 ? (states[1] ? "ON" : "OFF") : "N/A",
        count > 2 ? (states[2] ? "ON" : "OFF") : "N/A",
        count > 3 ? (states[3] ? "ON" : "OFF") : "N/A");

    return true;
  } catch (...) {
    LOG_INFO(LogTag::SYSTEM, "[PERSIST] saveRelayStates: exception writing preferences");
    return false;
  }
}

}  // namespace PersistentStorage
