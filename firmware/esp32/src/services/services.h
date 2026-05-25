#pragma once

#include "../core/events.h"
#include "../core/settings_store.h"

namespace Services {

void begin(const DeviceSettings &settings);
void update(uint32_t nowMs);
void handleEvent(const AppEvent &event, uint32_t nowMs);

}  // namespace Services
