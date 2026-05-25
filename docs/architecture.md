# DeskDroid API-First Architecture

DeskDroid is a dual-MCU firmware with a strict control-plane/execution-plane split.

```text
UI
  |
Application commands
  |
Canonical SystemState
  |
Services
  |
ESP8266 protocol transport
  |
Hardware execution
```

## Module Ownership

ESP32 owns UI, navigation, command validation, canonical state, WiFi, OTA-ready settings, reminders, timers, persistence, automation, and ESP8266 orchestration.

ESP8266 owns LED rendering, animation runtime, relay output application, deterministic frame timing, heartbeat responses, command queueing, and mirrored execution state. It must not make product decisions such as when an alarm starts, which animation means timer done, or whether settings are valid.

## Refactored Folder Structure

```text
firmware/esp32/src/
  app/
    application.cpp
    application_commands.{h,cpp}
    settings_flow.{h,cpp}
  core/
    events.{h,cpp}
    system_state.{h,cpp}
    settings_store.{h,cpp}
  services/
    audio_service.{h,cpp}
    connectivity_service.{h,cpp}
    lighting_service.{h,cpp}
    protocol_service.{h,cpp}
    settings_service.{h,cpp}
    timer_service.{h,cpp}
    services.{h,cpp}
  protocol/
    esp8266_link.{h,cpp}
    packet_builder.{h,cpp}
    synchronization_manager.{h,cpp}
  ui/
    screens.{h,cpp}

firmware/esp8266/src/
  protocol/
  state/
  led/
  relay/
  system/
```

## State Flow

Example: brightness change.

1. UI edits `LED Brightness` in `SettingsFlow`.
2. `SettingsFlow` calls `AppCommands::setBrightnessLevel`.
3. `SystemStateStore` updates `SystemState.lighting.brightness`.
4. `SystemStateStore` emits `EVENT_STATE_CHANGED` with `StateChange::Lighting`.
5. `ProtocolService` requests an ESP8266 state sync.
6. `Esp8266Link` sends `<FULL_SYNC|SEQ=n|...|BR=value|...>`.
7. ESP8266 applies the mirror state and replies `<SYNC_OK|SEQ=n>`.
8. ESP32 records the confirmed protocol revision in `SystemState.protocol`.
9. UI reads canonical state through feature/screen data, not through drivers.

## Command/API Layer

All frontends must call `AppCommands`, including the physical UI today and serial, mobile, backend, or OTA integrations later.

Implemented command entry points include:

- `setBrightness(uint8_t)`
- `setBrightnessLevel(uint8_t)`
- `setAnimationMode(AnimationMode)`
- `setColor(RGBColor)`
- `setLedMode(LedState)`
- `setIdlePreset(LedIdlePreset)`
- `startTimer(uint32_t)`
- `pauseTimer(uint32_t)`
- `stopTimer()`
- `resetTimer()`
- `setTimerDuration(uint32_t)`
- `setReminder(...)`
- `syncClock(...)`
- `setVolume(uint8_t)`
- `setMuted(bool)`
- `connectWifi(...)`
- `applySettings(...)`
- `saveSettings(...)`

## Service Layer

- `LightingService`: converts settings and time schedule into canonical lighting/backlight state.
- `TimerService`: updates timer completion and reacts to timer events.
- `AudioService`: owns buzzer execution and respects canonical audio state.
- `ConnectivityService`: mirrors WiFi status into `SystemState.connectivity`.
- `SettingsService`: persistence facade around settings load/save.
- `ProtocolService`: owns ESP8266 synchronization requests and link status.

The ESP32 local hardware request queue is now limited to ESP32-local hardware such as LCD and buzzer. It no longer renders NeoPixels or calls protocol mutators directly.

## ESP8266 Protocol Architecture

`Esp8266Link` is the transport/synchronization layer. It handles packet framing, sequence IDs for heartbeat/sync packets, heartbeat supervision, retry timing, reconnect, timeout recovery, and authoritative full-state synchronization.

The ESP8266 command queue still serializes execution-plane packet handling. It accepts compatible old packets plus sequenced packets for `PING` and `FULL_SYNC`, and it echoes `SEQ=` in `PONG`, `ACK`, and `SYNC_OK` responses when present.

## Anti-Patterns Found

- Timer state lived in private feature globals.
- Settings UI directly requested LED protocol/hardware changes.
- ESP32 hardware request execution touched NeoPixel rendering and ESP8266 link mutators.
- Protocol layer exposed state mutation functions instead of behaving as transport.
- UI/application logic triggered buzzer hardware requests directly in many handlers.
- Canonical state only covered a small LED/relay subset.

## Migration Strategy

1. Keep existing UI screens stable while redirecting writes through `AppCommands`.
2. Move feature globals into `SystemState` one domain at a time.
3. Let services subscribe to `EVENT_STATE_CHANGED`.
4. Keep ESP8266 as a mirror-only execution engine.
5. Add serialization and diff sync on top of `SystemState` revisions.
6. Migrate future serial/mobile/backend commands to `AppCommands` without new business logic.
7. Remove unused legacy ESP32 NeoPixel driver files after hardware validation confirms LED output is fully delegated.

## Example API Usage

```cpp
AppCommands::setBrightnessLevel(7);
AppCommands::setColor(RGBColor(255, 40, 0));
AppCommands::setAnimationMode(AnimationMode::Breathing);
AppCommands::startTimer(millis());
AppCommands::saveSettings(SettingsFlow::settings());
```

## Suggested Refactors

- Add unit-style host tests for `SystemStateStore` mutators and event emission.
- Add protocol tests for `SEQ=` ACK/NACK and retry behavior.
- Move reminders into a `ReminderState` domain next.
- Replace full sync with revisioned diffs after beta hardware is stable.
- Remove ESP32 NeoPixel dependency from `platformio.ini` once the legacy driver is deleted.
