#include "parser.h"

#include <string.h>

namespace Esp32ProtocolParser {

bool isAckPacket(const char* packet) {
  return packet != nullptr && strncmp(packet, "<ACK|", 5) == 0;
}

}  // namespace Esp32ProtocolParser

