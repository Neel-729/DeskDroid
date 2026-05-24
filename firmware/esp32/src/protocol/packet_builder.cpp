#include "packet_builder.h"

#include <stdio.h>

namespace PacketBuilder {

size_t ping(char* buffer, size_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0) {
    return 0;
  }

  const int written = snprintf(buffer, bufferSize, "<PING>");
  return written > 0 ? static_cast<size_t>(written) : 0;
}

}  // namespace PacketBuilder

