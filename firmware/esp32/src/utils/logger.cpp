#include "logger.h"

namespace Logger {

void info(Stream& stream, const __FlashStringHelper* message) {
  stream.println(message);
}

}  // namespace Logger

