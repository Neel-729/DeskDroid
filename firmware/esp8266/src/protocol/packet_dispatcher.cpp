#include "packet_dispatcher.h"

#include <string.h>

PacketDispatcher::PacketDispatcher(Stream& stream, RelayManager& relayManager, LedEngine& ledEngine)
    : stream_(stream), relayManager_(relayManager), ledEngine_(ledEngine) {}

void PacketDispatcher::dispatch(const Packet& packet) {
  const char* command = packet.command();

  if (equals(command, "PING")) {
    if (packet.tokenCount != 1) {
      sendError(ProtocolError::InvalidPacket);
      return;
    }
    sendAck(command);
    return;
  }

  if (equals(command, "SET_RELAY")) {
    if (packet.tokenCount != 3) {
      sendError(ProtocolError::InvalidPacket);
      return;
    }

    uint8_t relayNumber = 0;
    if (!parseRelayNumber(packet.tokens[1], relayNumber)) {
      sendError(ProtocolError::InvalidArgument);
      return;
    }

    bool enabled = false;
    if (equals(packet.tokens[2], "ON")) {
      enabled = true;
    } else if (equals(packet.tokens[2], "OFF")) {
      enabled = false;
    } else {
      sendError(ProtocolError::InvalidArgument);
      return;
    }

    if (!relayManager_.setRelay(relayNumber, enabled)) {
      sendError(ProtocolError::InvalidArgument);
      return;
    }

    sendAck(command);
    return;
  }

  if (equals(command, "SET_COLOR")) {
    if (packet.tokenCount != 4) {
      sendError(ProtocolError::InvalidPacket);
      return;
    }

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    if (!parseByte(packet.tokens[1], r) || !parseByte(packet.tokens[2], g) ||
        !parseByte(packet.tokens[3], b)) {
      sendError(ProtocolError::InvalidArgument);
      return;
    }

    ledEngine_.setSolidColor(r, g, b);
    sendAck(command);
    return;
  }

  if (equals(command, "CLEAR_LEDS")) {
    if (packet.tokenCount != 1) {
      sendError(ProtocolError::InvalidPacket);
      return;
    }

    ledEngine_.clear();
    sendAck(command);
    return;
  }

  sendError(ProtocolError::UnknownCommand);
}

bool PacketDispatcher::parseRelayNumber(const char* value, uint8_t& relayNumber) const {
  uint8_t parsed = 0;
  if (!parseByte(value, parsed)) {
    return false;
  }
  if (parsed == 0 || parsed > Config::RelayCount) {
    return false;
  }
  relayNumber = parsed;
  return true;
}

bool PacketDispatcher::parseByte(const char* value, uint8_t& result) const {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }

  uint16_t parsed = 0;
  for (const char* cursor = value; *cursor != '\0'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') {
      return false;
    }

    parsed = (parsed * 10) + static_cast<uint16_t>(*cursor - '0');
    if (parsed > 255) {
      return false;
    }
  }

  result = static_cast<uint8_t>(parsed);
  return true;
}

bool PacketDispatcher::equals(const char* lhs, const char* rhs) const {
  return strcmp(lhs, rhs) == 0;
}

void PacketDispatcher::sendAck(const char* command) {
  stream_.print(F("<ACK|"));
  stream_.print(command);
  stream_.println(F(">"));
}

void PacketDispatcher::sendError(ProtocolError error) {
  stream_.print(F("<ERR|"));
  stream_.print(errorText(error));
  stream_.println(F(">"));
}

const __FlashStringHelper* PacketDispatcher::errorText(ProtocolError error) const {
  switch (error) {
    case ProtocolError::InvalidPacket:
      return F("INVALID_PACKET");
    case ProtocolError::PacketTooLong:
      return F("PACKET_TOO_LONG");
    case ProtocolError::UnknownCommand:
      return F("UNKNOWN_COMMAND");
    case ProtocolError::InvalidArgument:
      return F("INVALID_ARGUMENT");
  }

  return F("UNKNOWN_ERROR");
}

