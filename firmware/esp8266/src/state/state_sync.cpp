#include "state_sync.h"

#include <string.h>

namespace {
constexpr uint16_t FieldR1 = 1 << 0;
constexpr uint16_t FieldR2 = 1 << 1;
constexpr uint16_t FieldR3 = 1 << 2;
constexpr uint16_t FieldR4 = 1 << 3;
constexpr uint16_t RequiredFields = FieldR1 | FieldR2 | FieldR3 | FieldR4;
}  // namespace

SyncParseResult StateSync::parseFullSync(const Packet& packet, StateSnapshot& snapshot) const {
  snapshot = {};

  if (!equals(packet.command(), "FULL_SYNC") || packet.tokenCount < 2) {
    return SyncParseResult::Invalid;
  }

  uint16_t fieldsSeen = 0;

  for (uint8_t i = 1; i < packet.tokenCount; ++i) {
    char* token = packet.tokens[i];
    char* separator = strchr(token, '=');
    if (separator == nullptr || separator == token || separator[1] == '\0') {
      return SyncParseResult::Invalid;
    }

    *separator = '\0';
    const char* key = token;
    const char* value = separator + 1;

    bool boolValue = false;
    if (equals(key, "SEQ")) {
      continue;
    } else if (equals(key, "R1")) {
      if (!parseBool(value, boolValue)) return SyncParseResult::Invalid;
      snapshot.relayStates[0] = boolValue;
      fieldsSeen |= FieldR1;
    } else if (equals(key, "R2")) {
      if (!parseBool(value, boolValue)) return SyncParseResult::Invalid;
      snapshot.relayStates[1] = boolValue;
      fieldsSeen |= FieldR2;
    } else if (equals(key, "R3")) {
      if (!parseBool(value, boolValue)) return SyncParseResult::Invalid;
      snapshot.relayStates[2] = boolValue;
      fieldsSeen |= FieldR3;
    } else if (equals(key, "R4")) {
      if (!parseBool(value, boolValue)) return SyncParseResult::Invalid;
      snapshot.relayStates[3] = boolValue;
      fieldsSeen |= FieldR4;
    } else {
      return SyncParseResult::Invalid;
    }
  }

  return (fieldsSeen & RequiredFields) == RequiredFields ? SyncParseResult::Ok
                                                         : SyncParseResult::Invalid;
}

bool StateSync::parseBool(const char* value, bool& result) const {
  if (equals(value, "1")) {
    result = true;
    return true;
  }
  if (equals(value, "0")) {
    result = false;
    return true;
  }
  return false;
}

bool StateSync::parseByte(const char* value, uint8_t& result) const {
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

bool StateSync::parseColor(const char* value, RgbColor& color) const {
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
  if (firstComma == nullptr) {
    return false;
  }
  *firstComma = '\0';

  char* secondComma = strchr(firstComma + 1, ',');
  if (secondComma == nullptr) {
    return false;
  }
  *secondComma = '\0';

  if (strchr(secondComma + 1, ',') != nullptr) {
    return false;
  }

  return parseByte(buffer, color.r) && parseByte(firstComma + 1, color.g) &&
         parseByte(secondComma + 1, color.b);
}

bool StateSync::parseEffect(const char* value, LedEffect& effect) const {
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

bool StateSync::equals(const char* lhs, const char* rhs) const {
  return strcmp(lhs, rhs) == 0;
}
