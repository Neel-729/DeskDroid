#pragma once

#include <Arduino.h>
#include "pins.h"

namespace Config {

constexpr uint32_t SerialBaud = 115200;

constexpr uint16_t NeoPixelCount = 83;
constexpr uint8_t DefaultBrightness = 64;

constexpr uint8_t RelayCount = 4;
constexpr uint8_t RelayActiveLevel = LOW;
constexpr uint8_t RelayInactiveLevel = HIGH;

constexpr size_t MaxPacketSize = 64;
constexpr uint8_t MaxSerialBytesPerUpdate = 32;
constexpr uint32_t HeartbeatIntervalMs = 1000;

}  // namespace Config
