#pragma once

#include <Arduino.h>

#include "config.h"

namespace Esp32ProtocolParser {

struct Packet {
  char* tokens[Config::Esp8266MaxPacketTokens] = {};
  uint8_t tokenCount = 0;

  const char* command() const;
};

enum class ParseResult : uint8_t {
  Ok,
  InvalidPacket,
};

ParseResult parse(char* packetBuffer, Packet &packet);
bool isAckPacket(const char* packet);
bool equals(const char* lhs, const char* rhs);

}  // namespace Esp32ProtocolParser
