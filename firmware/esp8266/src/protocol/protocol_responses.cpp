#include "protocol_responses.h"

namespace ProtocolResponse {

void sendAck(Stream& stream, const char* command) {
  stream.print(F("<ACK|"));
  stream.print(command);
  stream.println(F(">"));
}

void sendError(Stream& stream, ProtocolError error) {
  stream.print(F("<ERR|"));
  stream.print(errorText(error));
  stream.println(F(">"));
}

const __FlashStringHelper* errorText(ProtocolError error) {
  switch (error) {
    case ProtocolError::InvalidPacket:
      return F("INVALID_PACKET");
    case ProtocolError::PacketTooLong:
      return F("PACKET_TOO_LONG");
    case ProtocolError::QueueFull:
      return F("QUEUE_FULL");
    case ProtocolError::UnknownCommand:
      return F("UNKNOWN_COMMAND");
    case ProtocolError::InvalidArgument:
      return F("INVALID_ARGUMENT");
    case ProtocolError::InvalidSync:
      return F("INVALID_SYNC");
    case ProtocolError::SyncTimeout:
      return F("SYNC_TIMEOUT");
    case ProtocolError::RuntimeStalled:
      return F("RUNTIME_STALLED");
  }

  return F("UNKNOWN_ERROR");
}

}  // namespace ProtocolResponse
