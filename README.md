# DeskDroid

DeskDroid is a two-firmware embedded desktop utility. An ESP32 owns the user interface, application state, clock, timers, reminders, settings, and link supervision. An ESP8266 is an output processor for four relays and a 100-pixel NeoPixel strip.

This repository contains the current firmware projects plus older source snapshots. The live firmware is under `firmware/`; `DeskDroidArchives/` is historical and is not built by the current PlatformIO projects.

## Current implementation

- ESP32 controller firmware version `2.8.1`.
- ESP8266 output firmware version `0.1.0`.
- 16x2 I2C LCD UI, DS1307 clock, rotary encoder, buzzer, and TTP229-BSF transition logging.
- Clock with rotating motivational quotes, countdown timer, stopwatch, and five stored reminders.
- Settings for backlight, LED preset/brightness, relay selection, light schedule, auto-return timeout, buzzer, quotes, time format, time, date, and firmware display.
- ESP32-to-ESP8266 ASCII UART protocol with heartbeat, relay synchronization, separate LED-state acknowledgments, retries, and recovery.
- ESP8266 NeoPixel effects: off, solid, breathing, rainbow, and ambient; relay outputs are active low.

The TTP229 input is currently read and logged as key press/release transitions. It is not connected to application actions in the live firmware.

## Architecture

```text
ESP32 input/UI -> event queue -> application commands -> SystemState
                                      |                 |
                                      v                 v
                                  local LCD/buzzer   UART link
                                                        |
                                                        v
                                              ESP8266 state cache
                                                /             \
                                             relays        NeoPixels
```

See [docs/architecture.md](docs/architecture.md), [docs/system_state.md](docs/system_state.md), and [docs/protocol_spec.md](docs/protocol_spec.md).

## Hardware

The verified software pin assignments and wiring assumptions are in [docs/HARDWARE.md](docs/HARDWARE.md). The repository does not verify power ratings, level shifting, relay isolation, or a complete schematic; do not infer those details from the firmware.

## Prerequisites

- PlatformIO Core or the PlatformIO VS Code extension.
- USB access to an `esp32doit-devkit-v1` and a `nodemcuv2` board.
- The connected peripherals described in [docs/HARDWARE.md](docs/HARDWARE.md).

## Build, upload, and monitor

Run commands from the repository root:

```text
pio run -d firmware/esp32
pio run -d firmware/esp8266

pio run -d firmware/esp32 -t upload
pio run -d firmware/esp8266 -t upload

pio device monitor -d firmware/esp32
pio device monitor -d firmware/esp8266
```

The projects use Arduino through PlatformIO. Build environments and library declarations are documented in [docs/BUILD.md](docs/BUILD.md). Uploading requires selecting the appropriate connected serial port when PlatformIO does not detect it automatically.

## Configuration and persistence

Compile-time values are in `firmware/esp32/include/config.h`, `firmware/esp32/include/pins.h`, `firmware/esp32/include/version.h`, and their ESP8266 counterparts. Runtime settings and reminders use ESP32 Preferences storage. Relay states use a separate ESP32 NVS namespace. See [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

## Testing

There are no project test cases in the repository; the `test/` directories contain PlatformIO templates/placeholders only. The available checks are the two firmware builds and hardware/manual verification. See [docs/TESTING.md](docs/TESTING.md).

## Deployment and recovery

Deployment is a local USB upload of each firmware image. No OTA, release packaging, rollback, or fleet deployment mechanism is implemented or verified. See [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md) and [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

## Repository map

```text
firmware/esp32/       ESP32 PlatformIO project
firmware/esp8266/     ESP8266 PlatformIO project
hardware/              Wiring and reserved hardware directories
docs/                  Current engineering documentation
DeskDroidArchives/    Historical firmware snapshots and changelogs
src/                   Older standalone/scaffold sources; not a current PlatformIO project
tools/                 Reserved tooling directories; no live scripts are present
```

## Development

Read [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) before changing firmware. In particular, keep the ESP32 state ownership and ESP8266 execution-plane boundary intact, and validate both PlatformIO environments after changes.

## License

DeskDroid is released under the MIT License; see [LICENSE](LICENSE).
