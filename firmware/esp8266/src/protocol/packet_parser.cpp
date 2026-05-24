#include "packet_parser.h"

#include <string.h>

namespace {
constexpr char Separator = '|';
}  // namespace

ParseResult PacketParser::parse(char* packetBuffer, Packet& packet) const {
  packet = {};

  if (packetBuffer == nullptr || packetBuffer[0] == '\0') {
    return ParseResult::InvalidPacket;
  }

  char* cursor = packetBuffer;
  while (cursor != nullptr) {
    if (packet.tokenCount >= Packet::MaxTokens) {
      return ParseResult::InvalidPacket;
    }

    packet.tokens[packet.tokenCount++] = cursor;

    char* separator = strchr(cursor, Separator);
    if (separator == nullptr) {
      break;
    }

    *separator = '\0';
    cursor = separator + 1;
  }

  for (uint8_t i = 0; i < packet.tokenCount; ++i) {
    if (packet.tokens[i] == nullptr || packet.tokens[i][0] == '\0') {
      return ParseResult::InvalidPacket;
    }
  }

  return ParseResult::Ok;
}

