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

constexpr size_t MaxPacketSize = 128;
constexpr uint8_t MaxPacketTokens = 12;
constexpr uint8_t MaxSerialBytesPerUpdate = 32;
constexpr uint8_t CommandQueueSize = 8;
constexpr uint8_t MaxCommandsPerUpdate = 1;

constexpr uint32_t HeartbeatIntervalMs = 1000;
constexpr uint32_t ConnectionTimeoutMs = 5000;
constexpr uint16_t LedFrameIntervalMs = 16;

#ifndef DESKDROID_ENABLE_LOGGING
#define DESKDROID_ENABLE_LOGGING 0
#endif

}  // namespace Config
