#include "protocol.h"

#include <string.h>

namespace {
constexpr char PacketStart = '<';
constexpr char PacketEnd = '>';
}  // namespace

Protocol::Protocol(Stream& stream, CommandQueue& commandQueue)
    : stream_(stream), commandQueue_(commandQueue) {}

void Protocol::begin() {
  resetPacket();
}

void Protocol::update() {
  uint8_t processed = 0;
  while (stream_.available() > 0 && processed < Config::MaxSerialBytesPerUpdate) {
    consume(static_cast<char>(stream_.read()));
    ++processed;
  }
}

void Protocol::reset() {
  resetPacket();
}

void Protocol::resetPacket() {
  packetLength_ = 0;
  packet_[0] = '\0';
  assembling_ = false;
}

void Protocol::consume(char c) {
  if (c == PacketStart) {
    packetLength_ = 0;
    packet_[0] = '\0';
    assembling_ = true;
    return;
  }

  if (!assembling_) {
    return;
  }

  if (c == PacketEnd) {
    packet_[packetLength_] = '\0';
    assembling_ = false;
    processPacket();
    return;
  }

  if (c == '\r' || c == '\n') {
    return;
  }

  if (packetLength_ >= Config::MaxPacketSize) {
    resetPacket();
    commandQueue_.clear();
    ProtocolResponse::sendError(stream_, ProtocolError::PacketTooLong);
    return;
  }

  if (!isAllowedPacketChar(c)) {
    resetPacket();
    commandQueue_.clear();
    ProtocolResponse::sendError(stream_, ProtocolError::InvalidPacket);
    return;
  }

  packet_[packetLength_++] = c;
  packet_[packetLength_] = '\0';
}

void Protocol::processPacket() {
  char validationBuffer[Config::MaxPacketSize + 1] = {};
  strncpy(validationBuffer, packet_, Config::MaxPacketSize);
  validationBuffer[Config::MaxPacketSize] = '\0';

  Packet packet;
  const ParseResult result = parser_.parse(validationBuffer, packet);
  if (result != ParseResult::Ok) {
    resetPacket();
    commandQueue_.clear();
    ProtocolResponse::sendError(stream_, ProtocolError::InvalidPacket);
    return;
  }

  if (!commandQueue_.enqueue(packet_)) {
    ProtocolResponse::sendError(stream_, ProtocolError::QueueFull);
    return;
  }
}

bool Protocol::isAllowedPacketChar(char c) const {
  return c >= 32 && c <= 126;
}
