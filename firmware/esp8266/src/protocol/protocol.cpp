#include "protocol.h"

namespace {
constexpr char PacketStart = '<';
constexpr char PacketEnd = '>';
}  // namespace

Protocol::Protocol(Stream& stream, RelayManager& relayManager, LedEngine& ledEngine)
    : stream_(stream), dispatcher_(stream, relayManager, ledEngine) {}

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
    dispatcher_.sendError(ProtocolError::PacketTooLong);
    return;
  }

  packet_[packetLength_++] = c;
  packet_[packetLength_] = '\0';
}

void Protocol::processPacket() {
  Packet packet;
  const ParseResult result = parser_.parse(packet_, packet);
  if (result != ParseResult::Ok) {
    dispatcher_.sendError(ProtocolError::InvalidPacket);
    return;
  }

  dispatcher_.dispatch(packet);
}
