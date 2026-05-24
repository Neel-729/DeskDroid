#include "settings_store.h"

#include <Preferences.h>

namespace {
Preferences prefs;
}

namespace SettingsStore {

void begin(){
  prefs.begin("desk", false);
}

void loadDeviceSettings(DeviceSettings &settings){
  settings.buzzer = prefs.getBool("buzzer", true);
  settings.quotes = prefs.getBool("quotes", true);
  settings.format24 = prefs.getBool("24h", true);
  settings.brightness = prefs.getUChar("bright", 1);
  settings.autoLights = prefs.getBool("autol", true);
  settings.lightsOnHour = prefs.getUChar("lonh", 7);
  settings.lightsOnMinute = prefs.getUChar("lonm", 0);
  settings.lightsOffHour = prefs.getUChar("loffh", 22);
  settings.lightsOffMinute = prefs.getUChar("loffm", 0);
  settings.ledBrightness = prefs.getUChar("ledb", 6);
  settings.idlePreset = prefs.getUChar("ledmode", 1);
}

void saveDeviceSettings(const DeviceSettings &settings){
  prefs.putBool("buzzer",settings.buzzer);
  prefs.putBool("quotes",settings.quotes);
  prefs.putBool("24h",settings.format24);
  prefs.putUChar("bright",settings.brightness);
  prefs.putUChar("ledb", settings.ledBrightness);
  prefs.putUChar("ledmode", settings.idlePreset);
  prefs.putBool("autol", settings.autoLights);
  prefs.putUChar("lonh", settings.lightsOnHour);
  prefs.putUChar("lonm", settings.lightsOnMinute);
  prefs.putUChar("loffh", settings.lightsOffHour);
  prefs.putUChar("loffm", settings.lightsOffMinute);
}

void loadReminders(ReminderRecord reminders[], uint8_t count){
  for(uint8_t i=0;i<count;i++){
    char key[6];
    snprintf(key,sizeof(key),"r%dh",i);
    reminders[i].hour = prefs.getUChar(key, 8);
    snprintf(key,sizeof(key),"r%dm",i);
    reminders[i].minute = prefs.getUChar(key, 0);
    snprintf(key,sizeof(key),"r%da",i);
    reminders[i].active = prefs.getBool(key, false);
  }
}

void saveReminders(const ReminderRecord reminders[], uint8_t count){
  for(uint8_t i=0;i<count;i++){
    char key[6];
    snprintf(key,sizeof(key),"r%dh",i);
    prefs.putUChar(key, reminders[i].hour);
    snprintf(key,sizeof(key),"r%dm",i);
    prefs.putUChar(key, reminders[i].minute);
    snprintf(key,sizeof(key),"r%da",i);
    prefs.putBool(key, reminders[i].active);
  }
}

}

