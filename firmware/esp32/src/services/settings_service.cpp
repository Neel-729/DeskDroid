#include "settings_service.h"

#include "../app/application_commands.h"
#include "../core/settings_store.h"

namespace SettingsService {

void load(DeviceSettings &settings){
  SettingsStore::loadDeviceSettings(settings);
  AppCommands::applySettings(settings, false);
}

void save(const DeviceSettings &settings){
  AppCommands::saveSettings(settings);
}

}  // namespace SettingsService
