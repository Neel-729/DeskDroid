#pragma once

#include <Arduino.h>

#include "config.h"
#include "command_queue.h"
#include "packet_parser.h"
#include "protocol_responses.h"

class Protocol {
 public:
  Protocol(Stream& stream, CommandQueue& commandQueue);

  void begin();
  void update();

 private:
  void resetPacket();
  void consume(char c);
  void processPacket();
  bool isAllowedPacketChar(char c) const;

  Stream& stream_;
  CommandQueue& commandQueue_;
  PacketParser parser_;

  char packet_[Config::MaxPacketSize + 1] = {};
  size_t packetLength_ = 0;
  bool assembling_ = false;
};
