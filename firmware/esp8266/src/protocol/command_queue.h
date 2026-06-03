#pragma once

#include <Arduino.h>

#include "config.h"
#include "packet_dispatcher.h"
#include "packet_parser.h"

class CommandQueue {
 public:
  explicit CommandQueue(PacketDispatcher& dispatcher);

  void begin();
  bool enqueue(const char* packetPayload);
  void update();
  void clear();

  uint8_t size() const;
  bool full() const;

 private:
  struct QueuedCommand {
    char payload[Config::MaxPacketSize + 1] = {};
  };

  PacketDispatcher& dispatcher_;
  PacketParser parser_;
  QueuedCommand queue_[Config::CommandQueueSize];
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t size_ = 0;
};
