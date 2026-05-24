# UART Protocol Spec

Initial packet format:

```text
<COMMAND|ARG1|ARG2>
```

Initial commands:

- `<PING>`
- `<HEARTBEAT>`
- `<SET_RELAY|1|ON>`
- `<SET_RELAY|1|OFF>`
- `<SET_COLOR|255|0|0>`
- `<SET_BRIGHTNESS|180>`
- `<SET_EFFECT|RAINBOW>`
- `<CLEAR_LEDS>`
- `<FULL_SYNC|R1=1|R2=0|R3=1|R4=0|BR=180|FX=RAINBOW|CR=255,0,0|LED=1>`

Responses:

- `<ACK|COMMAND>`
- `<ERR|ERROR>`
- `<BOOT_READY>`
- `<SYNC_OK>`

Runtime states:

ESP8266:

- `BOOTING`
- `WAITING_FOR_SYNC`
- `SYNCING`
- `RUNNING`
- `DISCONNECTED`

ESP32 link:

- `DISCONNECTED`
- `CONNECTING`
- `SYNCING`
- `RUNNING`
- `ERROR`

The ESP32 is authoritative. The ESP8266 applies `FULL_SYNC` packets to rebuild its mirror state and restore relay/LED outputs.

The ESP32 sends `<PING>` every 2 seconds while supervising the execution plane. It accepts `<PONG>` and the compatibility response `<ACK|PING>` as liveness confirmation.
