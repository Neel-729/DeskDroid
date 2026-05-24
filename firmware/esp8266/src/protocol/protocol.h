#pragma once

#include <Arduino.h>

#include "config.h"
#include "../led/led_engine.h"
#include "packet_dispatcher.h"
#include "packet_parser.h"
#include "../relay/relay_manager.h"

class Protocol {
 public:
  Protocol(Stream& stream, RelayManager& relayManager, LedEngine& ledEngine);

  void begin();
  void update();

 private:
  static constexpr uint8_t MaxTokens = 4;

  void resetPacket();
  void consume(char c);
  void processPacket();

  Stream& stream_;
  PacketParser parser_;
  PacketDispatcher dispatcher_;

  char packet_[Config::MaxPacketSize + 1] = {};
  size_t packetLength_ = 0;
  bool assembling_ = false;
};
