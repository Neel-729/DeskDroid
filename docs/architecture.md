# DeskDroid Architecture

DeskDroid is migrating toward a dual-MCU architecture.

- ESP32: main controller, UI, input, automation, source-of-truth state.
- ESP8266: output processor for relays, NeoPixels, UART command handling, and output synchronization.

The first migration milestone keeps the ESP8266 additive and non-destructive.

