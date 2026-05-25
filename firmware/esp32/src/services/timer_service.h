#pragma once

#include "../core/events.h"

namespace TimerService {

void update(uint32_t nowMs);
void handleEvent(const AppEvent &event, uint32_t nowMs);

}  // namespace TimerService
