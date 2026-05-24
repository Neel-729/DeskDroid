#pragma once

#include <Arduino.h>

#include "../protocol/packet_parser.h"
#include "state_cache.h"

enum class SyncParseResult : uint8_t {
  Ok,
  Invalid,
};

class StateSync {
 public:
  SyncParseResult parseFullSync(const Packet& packet, StateSnapshot& snapshot) const;

 private:
  bool parseBool(const char* value, bool& result) const;
  bool parseByte(const char* value, uint8_t& result) const;
  bool parseColor(const char* value, RgbColor& color) const;
  bool parseEffect(const char* value, LedEffect& effect) const;
  bool equals(const char* lhs, const char* rhs) const;
};

