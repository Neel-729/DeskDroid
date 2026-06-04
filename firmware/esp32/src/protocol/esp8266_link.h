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

enum class LedApplyStatus : uint8_t {
  Pending,
  Applied,
  Failed,
};

struct Esp8266LinkDiagnostics {
  Esp8266ConnectionState state = Esp8266ConnectionState::Disconnected;
  uint8_t retryCount = 0;
  uint8_t recoveryAttemptCount = 0;
  uint16_t transportResetCount = 0;
  uint32_t lastRecoveryMs = 0;
  const char* lastRecoveryReason = "";
  const char* lastSyncReason = "";
  const char* desiredLedMode = "NONE";
  const char* lastSentLedMode = "NONE";
  const char* pendingLedMode = "NONE";
  bool desiredLedPower = false;
  bool lastSentLedPower = false;
  bool pendingLedPower = false;
  uint8_t desiredLedBrightness = 0;
  uint8_t lastSentLedBrightness = 0;
  uint8_t pendingLedBrightness = 0;
  uint8_t desiredLedSpeed = 0;
  uint8_t lastSentLedSpeed = 0;
  uint8_t pendingLedSpeed = 0;
  LedApplyStatus ledStatus = LedApplyStatus::Applied;
  uint8_t ledRetryCount = 0;
  uint32_t lastLedAckMs = 0;
  const char* lastLedErrReason = "";
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
const char* ledStatusName(LedApplyStatus status);

}  // namespace Esp8266Link
