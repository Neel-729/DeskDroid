#include "packet_dispatcher.h"

#include "protocol_responses.h"
#include "../utils/logger.h"

#include <string.h>

PacketDispatcher::PacketDispatcher(Stream& stream,
                                   StateCache& state,
                                   RelayManager& relayManager,
                                   LedEngine& ledEngine,
                                   Runtime& runtime)
    : stream_(stream),
      state_(state),
      relayManager_(relayManager),
      ledEngine_(ledEngine),
      runtime_(runtime) {}

void PacketDispatcher::dispatch(const Packet& packet) {
  const char* command = packet.command();

  if (equals(command, "PING")) {
    handlePing(packet);
    return;
  }

  if (equals(command, "HEARTBEAT") || equals(command, "HB")) {
    handleHeartbeat(packet);
    return;
  }

  if (equals(command, "FULL_SYNC")) {
    handleFullSync(packet);
    return;
  }

  if (equals(command, "SET_RELAY")) {
    handleSetRelay(packet);
    return;
  }

  if (equals(command, "SET_COLOR")) {
    handleSetColor(packet);
    return;
  }

  if (equals(command, "CLEAR_LEDS")) {
    handleClearLeds(packet);
    return;
  }

  if (equals(command, "SET_BRIGHTNESS")) {
    handleSetBrightness(packet);
    return;
  }

  if (equals(command, "SET_EFFECT")) {
    handleSetEffect(packet);
    return;
  }

  sendError(ProtocolError::UnknownCommand);
}

void PacketDispatcher::handleFullSync(const Packet& packet) {
  StateSnapshot snapshot;
  if (stateSync_.parseFullSync(packet, snapshot) != SyncParseResult::Ok) {
    sendError(ProtocolError::InvalidSync);
    return;
  }

  runtime_.beginSync();
  state_.applySnapshot(snapshot);
  relayManager_.applyStateCache();
  relayManager_.update();
  ledEngine_.applyState();
  runtime_.completeSync();
  stream_.println(F("<SYNC_OK>"));
  Logger::info(stream_, F("[SYNC]"), F("full sync applied"));
}

void PacketDispatcher::handlePing(const Packet& packet) {
  if (packet.tokenCount != 1) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  runtime_.recordHeartbeat();
  stream_.println(F("<PONG>"));
}

void PacketDispatcher::handleHeartbeat(const Packet& packet) {
  if (packet.tokenCount != 1) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  runtime_.recordHeartbeat();
  ProtocolResponse::sendAck(stream_, packet.command());
}

void PacketDispatcher::handleSetRelay(const Packet& packet) {
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

  runtime_.recordHeartbeat();
  ProtocolResponse::sendAck(stream_, packet.command());
}

void PacketDispatcher::handleSetColor(const Packet& packet) {
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
  runtime_.recordHeartbeat();
  ProtocolResponse::sendAck(stream_, packet.command());
}

void PacketDispatcher::handleClearLeds(const Packet& packet) {
  if (packet.tokenCount != 1) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  ledEngine_.clear();
  runtime_.recordHeartbeat();
  ProtocolResponse::sendAck(stream_, packet.command());
}

void PacketDispatcher::handleSetBrightness(const Packet& packet) {
  if (packet.tokenCount != 2) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  uint8_t brightness = 0;
  if (!parseByte(packet.tokens[1], brightness)) {
    sendError(ProtocolError::InvalidArgument);
    return;
  }

  ledEngine_.setBrightness(brightness);
  runtime_.recordHeartbeat();
  ProtocolResponse::sendAck(stream_, packet.command());
}

void PacketDispatcher::handleSetEffect(const Packet& packet) {
  if (packet.tokenCount != 2) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  LedEffect effect = LedEffect::None;
  if (!parseEffect(packet.tokens[1], effect)) {
    sendError(ProtocolError::InvalidArgument);
    return;
  }

  ledEngine_.setEffect(effect);
  runtime_.recordHeartbeat();
  ProtocolResponse::sendAck(stream_, packet.command());
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

bool PacketDispatcher::parseEffect(const char* value, LedEffect& effect) const {
  if (equals(value, "NONE") || equals(value, "OFF")) {
    effect = LedEffect::None;
    return true;
  }
  if (equals(value, "SOLID")) {
    effect = LedEffect::Solid;
    return true;
  }
  if (equals(value, "BREATH") || equals(value, "BREATHING")) {
    effect = LedEffect::Breathing;
    return true;
  }
  if (equals(value, "RAINBOW")) {
    effect = LedEffect::Rainbow;
    return true;
  }
  return false;
}

bool PacketDispatcher::equals(const char* lhs, const char* rhs) const {
  return strcmp(lhs, rhs) == 0;
}

void PacketDispatcher::sendError(ProtocolError error) {
  ProtocolResponse::sendError(stream_, error);
}
