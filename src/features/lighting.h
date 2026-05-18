#pragma once

#include "../core/settings_store.h"

namespace LightingFeature {
void refresh(const DeviceSettings &settings);
bool allowsOutput();
}
