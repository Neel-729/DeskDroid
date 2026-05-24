#include "esp8266_link.h"

namespace {
Stream* linkStream = nullptr;
}

namespace Esp8266Link {

void begin(Stream& stream) {
  linkStream = &stream;
}

void update() {}

bool isLinked() {
  return linkStream != nullptr;
}

}  // namespace Esp8266Link

