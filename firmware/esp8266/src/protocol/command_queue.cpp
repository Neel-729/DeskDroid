#include "command_queue.h"

#include <string.h>

CommandQueue::CommandQueue(PacketDispatcher& dispatcher) : dispatcher_(dispatcher) {}

void CommandQueue::begin() {
  head_ = 0;
  tail_ = 0;
  size_ = 0;
}

bool CommandQueue::enqueue(const char* packetPayload) {
  if (packetPayload == nullptr || full()) {
    return false;
  }

  strncpy(queue_[tail_].payload, packetPayload, Config::MaxPacketSize);
  queue_[tail_].payload[Config::MaxPacketSize] = '\0';
  tail_ = (tail_ + 1) % Config::CommandQueueSize;
  ++size_;
  return true;
}

void CommandQueue::update() {
  uint8_t processed = 0;

  while (size_ > 0 && processed < Config::MaxCommandsPerUpdate) {
    Packet packet;
    const ParseResult result = parser_.parse(queue_[head_].payload, packet);
    if (result == ParseResult::Ok) {
      dispatcher_.dispatch(packet);
    } else {
      dispatcher_.sendError(ProtocolError::InvalidPacket);
    }

    queue_[head_].payload[0] = '\0';
    head_ = (head_ + 1) % Config::CommandQueueSize;
    --size_;
    ++processed;
  }
}

uint8_t CommandQueue::size() const {
  return size_;
}

bool CommandQueue::full() const {
  return size_ >= Config::CommandQueueSize;
}

