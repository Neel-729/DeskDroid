# System State

The ESP32 is the source of truth. All subsystems write through `SystemStateStore`; consumers read `SystemStateStore::current()`.

## Canonical Domains

```cpp
struct SystemState {
  bool relayStates[4];
  LightingState lighting;
  TimerState timer;
  ConnectivityState connectivity;
  AudioState audio;
  SettingsState settings;
  ProtocolState protocol;
};
```

`LightingState` owns enabled/scheduled output, brightness, animation mode, color, semantic LED mode, and idle preset.

`TimerState` owns active/alarm state, duration, remaining time, clock edit fields, and alarm timing.

`ConnectivityState` owns WiFi and ESP8266 link status.

`AudioState` owns volume, mute, and buzzer enablement.

`SettingsState` owns the loaded settings snapshot and dirty flag.

`ProtocolState` owns ESP8266 confirmed sync metadata.

## Update Rules

- Do not mutate `SystemState` directly.
- Use `AppCommands` for product-facing actions.
- Use `SystemStateStore` mutators only inside command and service code.
- Every meaningful mutation increments `revision()`.
- Mutations emit `EVENT_STATE_CHANGED` with a `StateChange` mask.
- Services react to state events and perform hardware/protocol work.

## Synchronization

The ESP32 sends authoritative `FULL_SYNC` packets built from `SystemState`. The ESP8266 stores only mirror state and confirms with `SYNC_OK`.

Confirmed sync updates `SystemState.protocol.lastSyncRevision` and `lastSequenceId`. This prepares the firmware for diff-based synchronization and persisted state serialization.
