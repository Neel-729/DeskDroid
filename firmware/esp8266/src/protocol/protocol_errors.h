#pragma once

#include <Arduino.h>

enum class ProtocolError : uint8_t {
  InvalidPacket,
  PacketTooLong,
  QueueFull,
  UnknownCommand,
  InvalidArgument,
  InvalidSync,
};

