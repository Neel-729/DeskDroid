# System State

The ESP32 remains the source of truth.

The ESP8266 stores only mirror state:

- relay states
- brightness
- current color
- active effect placeholder
- LED enabled state
- runtime connection state

The ESP8266 enters `RUNNING` only after a successful `FULL_SYNC`.

## ESP32 Authoritative State

The ESP32 canonical state is represented by `SystemState`:

- `relayStates[4]`
- `ledsEnabled`
- `brightness`
- `currentColor`
- `currentEffect`

UI and feature code should mutate state through the ESP32 control-plane path. The execution plane is synchronized from that state rather than becoming an independent decision maker.
