# Configuration and persistence

## Build-time configuration

### ESP32

- `firmware/esp32/platformio.ini`: board, Arduino framework, upload speed `921600`, monitor speed `115200`, library dependencies, and build flags.
- `firmware/esp32/include/config.h`: NeoPixel count, UART packet limits, heartbeat/sync/recovery timing, and UART monitor flags.
- `firmware/esp32/include/pins.h`: GPIO assignments.
- `firmware/esp32/include/version.h`: firmware `2.8.1`, hardware revision `1.0`, and protocol version `1`.
- `firmware/esp32/partitions.csv`: an OTA-capable partition layout is present, but no OTA code or upload workflow is implemented.

The default build flags disable control and fault logging, while UART monitoring is enabled in normal mode (`DESKDROID_UART_MONITOR_ENABLED=1`, `DESKDROID_UART_MONITOR_MODE=0`).

### ESP8266

- `firmware/esp8266/platformio.ini`: board, Arduino framework, upload/monitor speeds, NeoPixel dependency, and `DESKDROID_ENABLE_LOGGING=0`.
- `firmware/esp8266/include/config.h`: relay count and active levels, LED count/brightness, packet/queue limits, heartbeat timeout, and LED frame interval.
- `firmware/esp8266/include/pins.h`: GPIO assignments.
- `firmware/esp8266/include/version.h`: firmware `0.1.0` and protocol label `0.1.0`.

## Runtime settings

The ESP32 settings UI exposes these entries:

| Setting | Stored key/behavior |
| --- | --- |
| Backlight | `bright`, boolean |
| LED mode | `ledmode`, 0–5 preset |
| LED brightness | `ledb`, 0–10 UI level; transmitted brightness is level × 25 |
| Relay | relay state is canonical state, not a device-settings key |
| Auto Lights | `autol` |
| Lights On/Off | `lonh`, `lonm`, `loffh`, `loffm` |
| Auto Return Home | In-memory `idleTimeoutSeconds` with options OFF, 15 s, 30 s, 60 s, 2 min, 5 min |
| Buzzer | `buzzer` |
| Quotes | `quotes` |
| Time Format | `24h` |
| Adjust Time/Date | Adjusts the DS1307 |
| About | Displays the ESP32 firmware version |

Settings and reminders use the `Preferences` namespace `desk`. Reminder records use generated keys `r0h`, `r0m`, `r0a` through `r4h`, `r4m`, `r4a`; inactive reminders default to 08:00.

The source currently does not save `idleTimeoutSeconds` in `SettingsStore::saveDeviceSettings`; it returns to the default 30-second in-memory value after reboot. This is an implementation limitation, not a deployment requirement.

## Relay persistence

`PersistentStorage` uses the separate `Preferences` namespace `DeskDroid` and key `relays`. Four relay states are packed into one byte, with relay 1 in bit 0 through relay 4 in bit 3. The ESP32 loads this state after `SystemStateStore::begin()` and before services start. A write occurs only when `AppCommands::setRelay` detects an actual state change. Missing or unreadable data leaves the default relay states off.

## Secrets and network configuration

No credentials, environment variables, or secret files are defined in the live projects. `AppCommands::connectWifi` accepts an SSID/password and calls `WiFi.begin`, but no caller, credential storage, or connection UI is present. Do not commit credentials if a future integration adds them.
