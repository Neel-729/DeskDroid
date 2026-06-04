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

  if (equals(command, "SET_LED_STATE")) {
    handleSetLedState(packet);
    return;
  }

  if (equals(command, "GET_LED_DIAG")) {
    handleLedDiagnostics(packet);
    return;
  }

  sendError(ProtocolError::UnknownCommand);
}

void PacketDispatcher::handleFullSync(const Packet& packet) {
  StateSnapshot snapshot;
  if (stateSync_.parseFullSync(packet, snapshot) != SyncParseResult::Ok) {
    runtime_.failSync();
    sendError(ProtocolError::InvalidSync);
    return;
  }

  runtime_.beginSync();
  state_.applySnapshot(snapshot);
  relayManager_.applyStateCache();
  relayManager_.update();
  runtime_.completeSync();
  sendSyncOkWithSequence(sequenceToken(packet));
  Logger::info(stream_, F("[SYNC]"), F("full sync applied"));
}

void PacketDispatcher::handlePing(const Packet& packet) {
  if (packet.tokenCount > 2) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  runtime_.recordHeartbeat();
  sendPongWithSequence(sequenceToken(packet));
}

void PacketDispatcher::handleHeartbeat(const Packet& packet) {
  if (packet.tokenCount > 2) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  runtime_.recordHeartbeat();
  sendAckWithSequence(packet.command(), sequenceToken(packet));
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
  sendAckWithSequence(packet.command(), sequenceToken(packet));
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
  sendAckWithSequence(packet.command(), sequenceToken(packet));
}

void PacketDispatcher::handleClearLeds(const Packet& packet) {
  if (packet.tokenCount != 1) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  ledEngine_.clear();
  runtime_.recordHeartbeat();
  sendAckWithSequence(packet.command(), sequenceToken(packet));
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
  sendAckWithSequence(packet.command(), sequenceToken(packet));
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
  sendAckWithSequence(packet.command(), sequenceToken(packet));
}

void PacketDispatcher::handleSetLedState(const Packet& packet) {
  if (packet.tokenCount < 6) {
    sendLedError("UNKNOWN", sequenceToken(packet), "INVALID_PACKET");
    return;
  }

  const char* sequence = nullptr;
  const char* modeName = "UNKNOWN";
  LedEffect effect = LedEffect::None;
  uint8_t brightness = 0;
  uint8_t speed = 0;
  bool power = false;
  RgbColor color;
  bool hasMode = false;
  bool hasBrightness = false;
  bool hasSpeed = false;
  bool hasPower = false;
  bool hasColor = false;

  for (uint8_t i = 1; i < packet.tokenCount; ++i) {
    const char* key = nullptr;
    const char* value = nullptr;
    if (!splitKeyValue(packet.tokens[i], key, value)) {
      sendLedError(modeName, sequence, "INVALID_TOKEN");
      ledEngine_.recordCommandResult("SET_LED_STATE", "ERR", "INVALID_TOKEN");
      return;
    }

    if (equals(key, "SEQ")) {
      sequence = value;
    } else if (equals(key, "MODE")) {
      modeName = value;
      if (!parseEffect(value, effect)) {
        sendLedError(modeName, sequence, "INVALID_MODE");
        ledEngine_.recordCommandResult("SET_LED_STATE", "ERR", "INVALID_MODE");
        return;
      }
      hasMode = true;
    } else if (equals(key, "BR")) {
      if (!parseByte(value, brightness)) {
        sendLedError(modeName, sequence, "INVALID_BRIGHTNESS");
        ledEngine_.recordCommandResult("SET_LED_STATE", "ERR", "INVALID_BRIGHTNESS");
        return;
      }
      hasBrightness = true;
    } else if (equals(key, "SPD")) {
      if (!parseByte(value, speed)) {
        sendLedError(modeName, sequence, "INVALID_SPEED");
        ledEngine_.recordCommandResult("SET_LED_STATE", "ERR", "INVALID_SPEED");
        return;
      }
      hasSpeed = true;
    } else if (equals(key, "PWR")) {
      if (!parseBool(value, power)) {
        sendLedError(modeName, sequence, "INVALID_POWER");
        ledEngine_.recordCommandResult("SET_LED_STATE", "ERR", "INVALID_POWER");
        return;
      }
      hasPower = true;
    } else if (equals(key, "CR")) {
      if (!parseColor(value, color)) {
        sendLedError(modeName, sequence, "INVALID_COLOR");
        ledEngine_.recordCommandResult("SET_LED_STATE", "ERR", "INVALID_COLOR");
        return;
      }
      hasColor = true;
    } else {
      sendLedError(modeName, sequence, "UNKNOWN_FIELD");
      ledEngine_.recordCommandResult("SET_LED_STATE", "ERR", "UNKNOWN_FIELD");
      return;
    }
  }

  if (!hasMode || !hasBrightness || !hasSpeed || !hasPower || !hasColor) {
    sendLedError(modeName, sequence, "MISSING_FIELD");
    ledEngine_.recordCommandResult("SET_LED_STATE", "ERR", "MISSING_FIELD");
    return;
  }

  ledEngine_.applyLedState(effect, brightness, speed, power, color);
  ledEngine_.recordCommandResult("SET_LED_STATE", "ACK", "");
  runtime_.recordHeartbeat();
  sendLedAck(LedEngine::effectName(effect), sequence);
  Logger::info(stream_, F("[LED_ACK]"), F("state applied"));
}

void PacketDispatcher::handleLedDiagnostics(const Packet& packet) {
  if (packet.tokenCount != 1) {
    sendError(ProtocolError::InvalidPacket);
    return;
  }

  sendLedDiagnostics();
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

bool PacketDispatcher::parseBool(const char* value, bool& result) const {
  if (equals(value, "1") || equals(value, "ON")) {
    result = true;
    return true;
  }
  if (equals(value, "0") || equals(value, "OFF")) {
    result = false;
    return true;
  }
  return false;
}

bool PacketDispatcher::parseColor(const char* value, RgbColor& color) const {
  if (value == nullptr) {
    return false;
  }

  char buffer[12] = {};
  const size_t length = strlen(value);
  if (length >= sizeof(buffer)) {
    return false;
  }
  strcpy(buffer, value);

  char* firstComma = strchr(buffer, ',');
  if (firstComma == nullptr) return false;
  *firstComma = '\0';

  char* secondComma = strchr(firstComma + 1, ',');
  if (secondComma == nullptr) return false;
  *secondComma = '\0';

  if (strchr(secondComma + 1, ',') != nullptr) return false;

  return parseByte(buffer, color.r) &&
         parseByte(firstComma + 1, color.g) &&
         parseByte(secondComma + 1, color.b);
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

bool PacketDispatcher::splitKeyValue(char* token, const char*& key, const char*& value) const {
  if (token == nullptr) {
    return false;
  }

  char* separator = strchr(token, '=');
  if (separator == nullptr || separator == token || separator[1] == '\0') {
    return false;
  }

  *separator = '\0';
  key = token;
  value = separator + 1;
  return true;
}

const char* PacketDispatcher::sequenceToken(const Packet& packet) const {
  for (uint8_t i = 1; i < packet.tokenCount; ++i) {
    if (strncmp(packet.tokens[i], "SEQ=", 4) == 0) {
      return packet.tokens[i] + 4;
    }
  }
  return nullptr;
}

void PacketDispatcher::sendAckWithSequence(const char* command, const char* sequence) {
  if (sequence == nullptr) {
    ProtocolResponse::sendAck(stream_, command);
    return;
  }

  stream_.print(F("<ACK|"));
  stream_.print(command);
  stream_.print(F("|SEQ="));
  stream_.print(sequence);
  stream_.println(F(">"));
}

void PacketDispatcher::sendLedAck(const char* mode, const char* sequence) {
  stream_.print(F("<ACK_LED_STATE"));
  if (sequence != nullptr) {
    stream_.print(F("|SEQ="));
    stream_.print(sequence);
  }
  stream_.print(F("|MODE="));
  stream_.print(mode != nullptr ? mode : "UNKNOWN");
  stream_.println(F(">"));
}

void PacketDispatcher::sendLedError(const char* mode, const char* sequence, const char* reason) {
  stream_.print(F("<ERR_LED_STATE"));
  if (sequence != nullptr) {
    stream_.print(F("|SEQ="));
    stream_.print(sequence);
  }
  stream_.print(F("|MODE="));
  stream_.print(mode != nullptr ? mode : "UNKNOWN");
  stream_.print(F("|REASON="));
  stream_.print(reason != nullptr ? reason : "UNKNOWN");
  stream_.println(F(">"));
}

void PacketDispatcher::sendLedDiagnostics() {
  const LedEngineDiagnostics& diagnostics = ledEngine_.diagnostics();
  stream_.print(F("<LED_DIAG|ACTIVE="));
  stream_.print(LedEngine::effectName(diagnostics.activeEffect));
  stream_.print(F("|PWR="));
  stream_.print(diagnostics.enabled ? 1 : 0);
  stream_.print(F("|BR="));
  stream_.print(diagnostics.brightness);
  stream_.print(F("|SPD="));
  stream_.print(diagnostics.speed);
  stream_.print(F("|RUNTIME="));
  stream_.print(diagnostics.animationRuntimeMs);
  stream_.print(F("|LAST="));
  stream_.print(diagnostics.lastCommand);
  stream_.print(F("|RESULT="));
  stream_.print(diagnostics.lastResult);
  stream_.print(F("|REASON="));
  stream_.print(diagnostics.lastReason);
  stream_.println(F(">"));
}

void PacketDispatcher::sendPongWithSequence(const char* sequence) {
  if (sequence == nullptr) {
    stream_.println(F("<PONG>"));
    return;
  }

  stream_.print(F("<PONG|SEQ="));
  stream_.print(sequence);
  stream_.println(F(">"));
}

void PacketDispatcher::sendSyncOkWithSequence(const char* sequence) {
  if (sequence == nullptr) {
    stream_.println(F("<SYNC_OK>"));
    return;
  }

  stream_.print(F("<SYNC_OK|SEQ="));
  stream_.print(sequence);
  stream_.println(F(">"));
}

bool PacketDispatcher::equals(const char* lhs, const char* rhs) const {
  return strcmp(lhs, rhs) == 0;
}

void PacketDispatcher::sendError(ProtocolError error) {
  ProtocolResponse::sendError(stream_, error);
}
