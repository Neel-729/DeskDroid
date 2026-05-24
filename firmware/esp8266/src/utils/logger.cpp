#include "logger.h"

#include "config.h"

namespace Logger {

void info(Stream& stream, const __FlashStringHelper* subsystem, const __FlashStringHelper* message) {
#if DESKDROID_ENABLE_LOGGING
  stream.print(F("<LOG|"));
  stream.print(subsystem);
  stream.print(F("|"));
  stream.print(message);
  stream.println(F(">"));
#else
  (void)stream;
  (void)subsystem;
  (void)message;
#endif
}

}  // namespace Logger
