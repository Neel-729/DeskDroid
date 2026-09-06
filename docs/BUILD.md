# Build and development setup

## Toolchain

Both firmware projects use PlatformIO with the Arduino framework:

| Project | Environment | Board | Framework |
| --- | --- | --- | --- |
| `firmware/esp32` | `esp32doit-devkit-v1` | DOIT ESP32 DEVKIT V1 | Arduino |
| `firmware/esp8266` | `nodemcuv2` | NodeMCU 1.0 (ESP-12E) | Arduino |

Dependencies are declared in each `platformio.ini`: LiquidCrystal_I2C, RTClib, and Adafruit NeoPixel for ESP32; Adafruit NeoPixel for ESP8266. PlatformIO resolves the framework, toolchains, and libraries.

## Commands

From the repository root:

```text
pio run -d firmware/esp32
pio run -d firmware/esp8266
pio run -d firmware/esp32 -t upload
pio run -d firmware/esp8266 -t upload
pio device monitor -d firmware/esp32
pio device monitor -d firmware/esp8266
```

Use `pio run -d <project> -t clean` to remove that project’s generated build output. Generated files are under each project’s `.pio/` directory and are ignored by Git.

The ESP32 monitor is configured for 115200 baud with the `esp32_exception_decoder` filter. The ESP8266 monitor is configured for 115200 baud.

## Build artifacts

PlatformIO generates the ELF and firmware binary under `.pio/build/<environment>/`. The ESP32 partition CSV defines `app0`, `app1`, NVS, OTA data, and SPIFFS partitions, but the repository does not contain an OTA installer.

## Development notes

The root `src/` directory is not a configured PlatformIO project. Build from the two `firmware/*` directories or use the `-d` commands above. The archived sources are not part of either build.
