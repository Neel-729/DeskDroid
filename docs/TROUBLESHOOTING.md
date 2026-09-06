# Troubleshooting

## Build cannot start because PlatformIO cannot lock its global cache

**Likely cause:** PlatformIO’s user-level package/cache directory is not writable or is owned by another account.

**How to verify:** The error names a file such as `platforms.lock` or a cache ownership/permission problem before compilation begins.

**Resolution:** Fix ownership/permissions for the PlatformIO installation/cache or run PlatformIO in an environment with a writable user cache. This is not a firmware source error.

## ESP32 remains in link recovery

**Likely cause:** Incorrect TX/RX wiring, missing common ground, wrong serial port/board firmware, or an ESP8266 that is not running.

**How to verify:** ESP32 diagnostics expose link state, retry/recovery counts, and UART TX/RX counters. The ESP8266 monitor should show `<BOOT_READY>` after reset.

**Resolution:** Confirm ESP32 GPIO17 → ESP8266 GPIO3, ESP32 GPIO16 ← ESP8266 GPIO1, common ground, 115200 baud, and that both projects were uploaded to the intended boards.

## ESP8266 rejects synchronization

**Likely cause:** A malformed or oversized framed packet, missing `R1`–`R4` field, invalid boolean, or protocol mismatch.

**How to verify:** Look for `<ERR|INVALID_SYNC>`, `<ERR|INVALID_PACKET>`, `[UART][ERR]`, or ESP32 fault code `PROTO_MALFORMED_PACKET`.

**Resolution:** Use the current `<FULL_SYNC|SEQ=n|R1=0|R2=0|R3=0|R4=0>` form. Do not use the obsolete byte/checksum packet format from earlier documentation.

## Relays are unexpectedly off after reboot

**Likely cause:** No `DeskDroid/relays` NVS key exists, NVS was erased, or the controller could not load the key.

**How to verify:** ESP32 logs `[PERSIST] No saved relay state found` or a load error. A successful restore logs `[PERSIST] Restoring relay states from NVS`.

**Resolution:** Set relay states through the application and retain ESP32 NVS across reset. A full flash erase intentionally removes the saved state.

## Relay output polarity is inverted

**Likely cause:** The external relay board uses a different active level than the firmware assumption.

**How to verify:** `firmware/esp8266/include/config.h` sets `RelayActiveLevel = LOW` and `RelayInactiveLevel = HIGH`.

**Resolution:** Verify the relay module electrically before changing firmware configuration. The repository only documents the current active-low assumption.

## LCD is blank or shows stale rows

**Likely cause:** I2C wiring/address mismatch or a backlight schedule/settings state that disables the backlight.

**How to verify:** Confirm SDA GPIO21, SCL GPIO22, LCD address `0x27`, and inspect the ESP32 monitor for initialization/fault output.

**Resolution:** Correct wiring/address and ensure the configured light schedule allows output. The driver caches rows and intentionally writes only changed characters.

## RTC error or incorrect time

**Likely cause:** DS1307 is missing/unresponsive or its oscillator is not running.

**How to verify:** `RtcDriver::begin()` failure produces the `RTC ERROR` screen. If `isrunning()` is false, setup adjusts the RTC to `__DATE__`/`__TIME__`.

**Resolution:** Check I2C wiring, RTC power, and module operation, then set time/date from Settings.

## LED state remains pending or enters error

**Likely cause:** ESP8266 link loss, invalid LED fields, or acknowledgments that do not match the pending sequence.

**How to verify:** Inspect ESP32 `LED_STATE` diagnostics for `WAITING_ACK`, retry count, stale acknowledgments, and last error reason.

**Resolution:** Restore the link and confirm the ESP8266 accepts `SET_LED_STATE`. After three failed retries, a new state change or link resynchronization is required to retry.

## Auto Return Home does not survive reboot

**Likely cause:** This is a current implementation limitation: `idleTimeoutSeconds` is not written by `SettingsStore`.

**How to verify:** The setting changes during the current session but returns to 30 seconds after restart.

**Resolution:** Re-select the desired timeout after boot. Persistent timeout storage is not currently implemented.
