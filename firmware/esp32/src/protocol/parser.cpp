#include "parser.h"

#include <string.h>

namespace Esp32ProtocolParser {

namespace {
constexpr char Separator = '|';
}

const char* Packet::command() const {
  return tokenCount > 0 && tokens[0] != nullptr ? tokens[0] : "";
}

ParseResult parse(char* packetBuffer, Packet &packet) {
  packet = {};

  if(packetBuffer == nullptr || packetBuffer[0] == '\0'){
    return ParseResult::InvalidPacket;
  }

  char* cursor = packetBuffer;
  while(cursor != nullptr){
    if(packet.tokenCount >= Config::Esp8266MaxPacketTokens){
      return ParseResult::InvalidPacket;
    }

    packet.tokens[packet.tokenCount++] = cursor;

    char* separator = strchr(cursor, Separator);
    if(separator == nullptr){
      break;
    }

    *separator = '\0';
    cursor = separator + 1;
  }

  for(uint8_t i = 0; i < packet.tokenCount; ++i){
    if(packet.tokens[i] == nullptr || packet.tokens[i][0] == '\0'){
      return ParseResult::InvalidPacket;
    }
  }

  return ParseResult::Ok;
}

bool isAckPacket(const char* packet) {
  return packet != nullptr && strncmp(packet, "<ACK|", 5) == 0;
}

bool equals(const char* lhs, const char* rhs) {
  return strcmp(lhs, rhs) == 0;
}

}  // namespace Esp32ProtocolParser
