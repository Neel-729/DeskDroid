# UART Protocol Spec

Initial packet format:

```text
<COMMAND|ARG1|ARG2>
```

Initial commands:

- `<PING>`
- `<SET_RELAY|1|ON>`
- `<SET_RELAY|1|OFF>`
- `<SET_COLOR|255|0|0>`
- `<CLEAR_LEDS>`

Responses:

- `<ACK|COMMAND>`
- `<ERR|ERROR>`

