#pragma once

#include <Arduino.h>

#include "protocol_errors.h"

namespace ProtocolResponse {

void sendAck(Stream& stream, const char* command);
void sendError(Stream& stream, ProtocolError error);
const __FlashStringHelper* errorText(ProtocolError error);

}  // namespace ProtocolResponse

