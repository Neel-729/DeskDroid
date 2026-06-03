#pragma once

#include <Arduino.h>

#include "../core/hardware_types.h"

enum class Esp8266ConnectionState : uint8_t {
  Disconnected,
  Connecting,
  Syncing,
  Running,
  Recovering,
  Error,
};

struct Esp8266LinkDiagnostics {
  Esp8266ConnectionState state = Esp8266ConnectionState::Disconnected;
  uint8_t retryCount = 0;
  uint8_t recoveryAttemptCount = 0;
  uint16_t transportResetCount = 0;
  uint32_t lastRecoveryMs = 0;
  const char* lastRecoveryReason = "";
  const char* lastSyncReason = "";
};

namespace Esp8266Link {

void begin();
void update();
bool isLinked();
bool isRunning();
Esp8266ConnectionState state();

void requestFullSync();
const Esp8266LinkDiagnostics& diagnostics();
const char* stateName(Esp8266ConnectionState state);

}  // namespace Esp8266Link
