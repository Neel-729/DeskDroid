#pragma once

#include "../core/events.h"
#include "../core/settings_store.h"

namespace LightingService {

void begin(const DeviceSettings &settings);
void refreshSchedule(const DeviceSettings &settings);
void handleEvent(const AppEvent &event);

}  // namespace LightingService
