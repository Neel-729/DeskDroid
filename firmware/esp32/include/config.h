#pragma once

namespace DeskDroid {

constexpr const char* ProjectName = "DeskDroid";

}  // namespace DeskDroid

namespace Config {

constexpr uint32_t Esp8266SerialBaud = 115200;
constexpr size_t Esp8266MaxPacketSize = 128;
constexpr uint8_t Esp8266MaxPacketTokens = 12;
constexpr uint8_t Esp8266MaxSerialBytesPerUpdate = 32;
constexpr uint32_t Esp8266HeartbeatIntervalMs = 2000;
constexpr uint32_t Esp8266HeartbeatTimeoutMs = 6500;
constexpr uint32_t Esp8266SyncRetryIntervalMs = 1000;
constexpr uint8_t Esp8266MaxSyncRetries = 5;

#ifndef DESKDROID_CONTROL_LOGGING
#define DESKDROID_CONTROL_LOGGING 0
#endif

}  // namespace Config
