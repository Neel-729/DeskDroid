# Testing and validation

## Repository test infrastructure

The PlatformIO test directories contain no test cases: `firmware/esp32/test/` has the default PlatformIO README, and `firmware/esp8266/test/` contains only `.gitkeep`. There are no host tests, protocol fixtures, integration tests, CI workflows, or coverage configuration in the repository.

The nominal commands are:

```text
pio test -d firmware/esp32
pio test -d firmware/esp8266
```

They currently terminate with PlatformIO’s `Nothing to build` error because no test suite exists; this is an absence of tests, not a passing test result.

## Build validation

The practical automated check is a release build of both environments:

```text
pio run -d firmware/esp32
pio run -d firmware/esp8266
```

These builds compile the current source and resolve the declared PlatformIO dependencies.

## Hardware validation checklist

Because behavior is hardware-dependent, verify at least:

1. ESP32 boots, shows the version/boot animation, and displays the clock.
2. A missing/unrunning DS1307 produces `RTC ERROR` or is adjusted to the compile timestamp as implemented.
3. Encoder rotation, click, double-click, and 1000 ms hold produce the expected UI transitions described in [USER_INTERFACE.md](USER_INTERFACE.md).
4. TTP229 key transitions appear in the ESP32 monitor; no application action should be expected from them yet.
5. ESP8266 emits `<BOOT_READY>`, accepts a relay `FULL_SYNC`, and returns `<SYNC_OK>`.
6. Relay outputs are inactive at ESP8266 initialization and then follow the synchronized state.
7. LED state changes receive `ACK_SUCCESS`; unplug/reconnect tests exercise bounded retry/recovery behavior.
8. Settings and reminders survive an ESP32 reboot; relay states survive when NVS is retained.

No hardware test results are stored in the repository. Do not claim pass status without running the applicable bench test.
