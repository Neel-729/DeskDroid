#pragma once

#include <Arduino.h>

#include "config.h"

struct Packet {
  static constexpr uint8_t MaxTokens = Config::MaxPacketTokens;

  char* tokens[MaxTokens] = {};
  uint8_t tokenCount = 0;

  const char* command() const {
    return tokenCount > 0 ? tokens[0] : "";
  }
};

enum class ParseResult : uint8_t {
  Ok,
  InvalidPacket,
};

class PacketParser {
 public:
  ParseResult parse(char* packetBuffer, Packet& packet) const;
};
