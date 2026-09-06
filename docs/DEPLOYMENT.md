# Deployment and updates

## Current deployment path

Deployment is a board-local USB/serial upload using PlatformIO:

```text
pio run -d firmware/esp32 -t upload
pio run -d firmware/esp8266 -t upload
```

Build and upload the ESP8266 output processor as well as the ESP32 controller. The source does not define a required upload order. After both boards boot, the ESP8266 emits `<BOOT_READY>` and the ESP32 performs relay synchronization.

## Version identification

- ESP32 version: `FIRMWARE_VERSION_STRING` in `firmware/esp32/include/version.h`, currently `2.8.1`.
- ESP8266 version: `Version::FirmwareVersion` in `firmware/esp8266/include/version.h`, currently `0.1.0`.
- ESP32 hardware revision: `1.0`; ESP32 protocol version constant: `1`.

## Recovery after upload

Open both 115200-baud monitors. Expected link milestones are ESP8266 `<BOOT_READY>`, ESP32 sync activity, `<SYNC_OK>`, and an ESP32 link state of `RUNNING`. If the link does not recover, use [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

## Not implemented

The ESP32 partition table contains OTA slots, but there is no download, signature/checksum validation, boot selection, rollback, release artifact, or remote deployment implementation in the repository. Do not describe the project as OTA-enabled.
