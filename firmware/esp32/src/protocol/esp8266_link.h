#pragma once

#include <Arduino.h>

#include "../core/hardware_types.h"

enum class Esp8266ConnectionState : uint8_t {
  Disconnected,
  Connecting,
  Syncing,
  Running,
  Error,
};

namespace Esp8266Link {

void begin();
void update();
bool isLinked();
bool isRunning();
Esp8266ConnectionState state();

void requestFullSync();

}  // namespace Esp8266Link
