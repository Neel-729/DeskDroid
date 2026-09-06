# System state

`SystemStateStore` is the ESP32 controller’s canonical in-memory state. `SystemStateStore::current()` exposes a read-only view to consumers; mutators increment a revision and enqueue `EVENT_STATE_CHANGED` with a `StateChange` mask unless the operation is a protocol metadata update.

## Domains

```cpp
struct SystemState {
  bool relayStates[4];
  LightingState lighting;
  TimerState timer;
  StopwatchState stopwatch;
  ConnectivityState connectivity;
  AudioState audio;
  SettingsState settings;
  ProtocolState protocol;
};
```

- `relayStates`: four controller-owned relay decisions.
- `lighting`: enabled/power policy, schedule permission, LCD backlight, byte brightness, effect mode, RGB color, semantic mode, and idle preset.
- `timer`: IDLE/EDITING/RUNNING/PAUSED/COMPLETE state, duration/remaining time, edit fields, end time, and alarm timing.
- `stopwatch`: IDLE/RUNNING/PAUSED state, start time, and elapsed time.
- `connectivity`: Wi-Fi status/RSSI and ESP8266 link status.
- `audio`: volume, mute, and buzzer enablement.
- `settings`: loaded `DeviceSettings` snapshot and dirty flag.
- `protocol`: ESP8266 connection flag and last confirmed sync revision/sequence.

## Mutation and synchronization rules

Product-facing callers use `AppCommands`; services and feature modules use state-store mutators. Lighting mutations are consumed by `LightingService` and the ESP8266 LED path. Relay mutations are persisted by `AppCommands::setRelay` and request relay synchronization. Timer and reminder mutations drive UI/alarm behavior on the ESP32.

The ESP32 sends relay state as `FULL_SYNC`. The ESP8266 stores only a `StateSnapshot` mirror for four relays plus LED state. `SYNC_OK` records the ESP32 revision and sequence metadata; it does not create another state-change event.

## Defaults

The state structures initialize to a five-minute timer, enabled solid lighting with RGB `(30,0,20)`, LED byte brightness 128, four relays off, and idle audio/connection states. Settings loading then applies Preferences defaults; see [CONFIGURATION.md](CONFIGURATION.md) for the persisted defaults and the timeout persistence limitation.
