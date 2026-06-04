#pragma once

#include <Arduino.h>

#include "config.h"
#include "packet_parser.h"
#include "protocol_errors.h"
#include "../led/led_engine.h"
#include "../relay/relay_manager.h"
#include "../state/state_cache.h"
#include "../state/state_sync.h"
#include "../system/runtime.h"

class PacketDispatcher {
 public:
  PacketDispatcher(Stream& stream,
                   StateCache& state,
                   RelayManager& relayManager,
                   LedEngine& ledEngine,
                   Runtime& runtime);

  void dispatch(const Packet& packet);
  void sendError(ProtocolError error);

 private:
  void handleFullSync(const Packet& packet);
  void handlePing(const Packet& packet);
  void handleHeartbeat(const Packet& packet);
  void handleSetRelay(const Packet& packet);
  void handleSetColor(const Packet& packet);
  void handleClearLeds(const Packet& packet);
  void handleSetBrightness(const Packet& packet);
  void handleSetEffect(const Packet& packet);
  void handleSetLedState(const Packet& packet);
  void handleLedDiagnostics(const Packet& packet);

  bool parseRelayNumber(const char* value, uint8_t& relayNumber) const;
  bool parseByte(const char* value, uint8_t& result) const;
  bool parseBool(const char* value, bool& result) const;
  bool parseColor(const char* value, RgbColor& color) const;
  bool parseEffect(const char* value, LedEffect& effect) const;
  bool splitKeyValue(char* token, const char*& key, const char*& value) const;
  const char* sequenceToken(const Packet& packet) const;
  void sendAckWithSequence(const char* command, const char* sequenceToken);
  void sendLedAck(const char* mode, const char* sequenceToken);
  void sendLedError(const char* mode, const char* sequenceToken, const char* reason);
  void sendLedDiagnostics();
  void sendPongWithSequence(const char* sequenceToken);
  void sendSyncOkWithSequence(const char* sequenceToken);
  bool equals(const char* lhs, const char* rhs) const;

  Stream& stream_;
  StateCache& state_;
  RelayManager& relayManager_;
  LedEngine& ledEngine_;
  Runtime& runtime_;
  StateSync stateSync_;
};
