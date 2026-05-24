#pragma once

#include "../core/hardware_types.h"
#include "../core/settings_store.h"

namespace LightingFeature {
void begin(const DeviceSettings &settings);
void refreshSchedule(const DeviceSettings &settings);
void refresh(const DeviceSettings &settings);

void requestMode(LedState mode);
LedState requestedMode();

bool allowsOutput();
bool backlightEnabled(const DeviceSettings &settings);
}
