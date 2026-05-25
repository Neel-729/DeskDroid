#pragma once

#include "../core/settings_store.h"

namespace SettingsService {

void load(DeviceSettings &settings);
void save(const DeviceSettings &settings);

}  // namespace SettingsService
