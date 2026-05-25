# UART Protocol Spec

Packet format:

```text
<COMMAND|ARG1|ARG2>
<COMMAND|SEQ=123|KEY=VALUE>
```

Supported commands:

- `<PING|SEQ=123>`
- `<HEARTBEAT>`
- `<SET_RELAY|1|ON>`
- `<SET_RELAY|1|OFF>`
- `<SET_COLOR|255|0|0>`
- `<SET_BRIGHTNESS|180>`
- `<SET_EFFECT|RAINBOW>`
- `<CLEAR_LEDS>`
- `<FULL_SYNC|SEQ=124|R1=1|R2=0|R3=1|R4=0|BR=180|FX=RAINBOW|CR=255,0,0|LED=1>`

Responses:

- `<ACK|COMMAND>`
- `<ACK|COMMAND|SEQ=123>`
- `<ERR|ERROR>`
- `<BOOT_READY>`
- `<SYNC_OK>`
- `<SYNC_OK|SEQ=124>`
- `<PONG|SEQ=123>`

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

The ESP32 sends sequenced `<PING|SEQ=n>` packets while supervising the execution plane. It accepts `<PONG>`, `<PONG|SEQ=n>`, and the compatibility response `<ACK|PING>` as liveness confirmation.

`SEQ=` is optional for compatibility with older packets. When present, the ESP8266 echoes it in the response. Full sync retries are driven by timeout and retry counters on the ESP32 control plane.
