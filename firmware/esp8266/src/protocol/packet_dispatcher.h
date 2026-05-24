#pragma once

#include <Arduino.h>

#include "config.h"
#include "packet_parser.h"
#include "../led/led_engine.h"
#include "../relay/relay_manager.h"

enum class ProtocolError : uint8_t {
  InvalidPacket,
  PacketTooLong,
  UnknownCommand,
  InvalidArgument,
};

class PacketDispatcher {
 public:
  PacketDispatcher(Stream& stream, RelayManager& relayManager, LedEngine& ledEngine);

  void dispatch(const Packet& packet);
  void sendError(ProtocolError error);

 private:
  bool parseRelayNumber(const char* value, uint8_t& relayNumber) const;
  bool parseByte(const char* value, uint8_t& result) const;
  bool equals(const char* lhs, const char* rhs) const;

  void sendAck(const char* command);
  const __FlashStringHelper* errorText(ProtocolError error) const;

  Stream& stream_;
  RelayManager& relayManager_;
  LedEngine& ledEngine_;
};
