#include "timers.h"

namespace Timers {

bool elapsed(uint32_t nowMs, uint32_t previousMs, uint32_t intervalMs) {
  return nowMs - previousMs >= intervalMs;
}

}  // namespace Timers

