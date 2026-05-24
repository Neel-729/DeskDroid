# DeskDroid Architecture

DeskDroid is migrating toward a dual-MCU architecture.

- ESP32: main controller, UI, input, automation, source-of-truth state.
- ESP8266: output processor for relays, NeoPixels, UART command handling, and output synchronization.

The first migration milestone keeps the ESP8266 additive and non-destructive.

## Control Plane Runtime

The ESP32 owns canonical runtime state and supervises the ESP8266 execution plane through a centralized `Esp8266Link` transport. No application or feature module should access `Serial2` directly.

Startup and recovery flow:

1. ESP8266 emits `<BOOT_READY>`.
2. ESP32 transitions to `SYNCING`.
3. ESP32 sends an authoritative `<FULL_SYNC|...>` packet.
4. ESP8266 applies mirror state and emits `<SYNC_OK>`.
5. ESP32 transitions to `RUNNING`.

If heartbeat supervision times out, the ESP32 marks the execution plane disconnected, preserves authoritative state, and waits for either `<BOOT_READY>` or a heartbeat response before resynchronizing.
