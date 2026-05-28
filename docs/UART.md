DeskDroid UART Communication Protocol Documentation

Overview

DeskDroid employs a dual-MCU architecture:

- ESP32 – Main controller, orchestrates system state, user commands, and application logic.
- ESP8266 – Dedicated output processor, responsible for deterministic execution of relays, NeoPixels, and other outputs.

Communication between the MCUs is handled via UART, providing:

- Reliable command dispatch
- State synchronization
- Event supervision (ping/pong)
- Error detection and recovery

The UART link is the single source of truth for ESP8266 regarding what outputs to execute, with the ESP32 dictating system state.

---

Hardware UART Setup

MCU	UART Pins	Notes	
ESP32	TX → RX2 (ESP8266), RX ← TX2 (ESP8266)	Standard UART, non-blocking	
ESP8266	RX ← TX2 (ESP32), TX → RX2 (ESP32)	Dedicated output channel	
Common	GND ↔ GND	Shared ground for proper signal reference	

- Baud Rate: 115200 bps (configurable)
- Packet Logic: Full-duplex communication using structured frames
- Physical Medium: Direct TTL-level UART

---

Protocol Structure

Each UART packet follows a frame-based structure:

```
[START_BYTE][CMD_ID][LENGTH][PAYLOAD][CHECKSUM][END_BYTE]
```

Fields

1. START_BYTE – `0xAA`
   - Marks the beginning of a packet.

2. CMD_ID – 1 byte
   - Identifies the command or message type (e.g., relay toggle, LED update, ping, state sync).

3. LENGTH – 1 byte
   - Number of bytes in the PAYLOAD.

4. PAYLOAD – variable
   - Command-specific data.

5. CHECKSUM – 1 byte
   - XOR of all bytes from CMD_ID to the last payload byte.

6. END_BYTE – `0x55`
   - Marks the end of a packet.

---

Command Types

CMD_ID	Name	Direction	Description	
`0x01`	PING	ESP32 → ESP8266	Health check, expects PONG reply	
`0x02`	PONG	ESP8266 → ESP32	Reply to PING, confirms MCU alive	
`0x10`	RELAY_CONTROL	ESP32 → ESP8266	Control a relay output (payload: relay ID + state)	
`0x11`	RELAY_STATUS	ESP8266 → ESP32	Report current relay state	
`0x20`	LED_UPDATE	ESP32 → ESP8266	Update NeoPixel LEDs (payload: pattern ID + params)	
`0x21`	LED_STATUS	ESP8266 → ESP32	Confirm LED update applied	
`0x30`	SYSTEM_STATE_SYNC	ESP32 → ESP8266	Push full system state for mirroring	
`0x31`	STATE_ACK	ESP8266 → ESP32	Acknowledge system state sync	
`0x40`	ERROR_REPORT	ESP8266 → ESP32	Report invalid packet or execution error	

---

Packet Examples

Relay ON Command (Relay ID: 2)

Byte	Value	
START_BYTE	`0xAA`	
CMD_ID	`0x10`	
LENGTH	`0x02`	
PAYLOAD	`0x02 0x01` (Relay 2 ON)	
CHECKSUM	`0x13`	
END_BYTE	`0x55`	

LED Update Command (Pattern ID: 5, Speed: `0x0A`)

Byte	Value	
START_BYTE	`0xAA`	
CMD_ID	`0x20`	
LENGTH	`0x02`	
PAYLOAD	`0x05 0x0A`	
CHECKSUM	`0x0F`	
END_BYTE	`0x55`	

---

Synchronization & Reliability

Ping/Pong Mechanism
ESP32 periodically sends PING to ESP8266. If no PONG is received, it retries up to N times before triggering error handling.

State Mirroring
All system-critical states (relays, LEDs, timers) are mirrored to ESP8266 via `SYSTEM_STATE_SYNC`. Any discrepancy triggers `STATE_ACK` validation.

Checksum Verification
Every packet uses a simple XOR checksum for error detection. Invalid packets are discarded and logged via `ERROR_REPORT`.

Queue Management
ESP8266 maintains a FIFO queue for commands, ensuring deterministic execution even if multiple commands arrive in quick succession.

---

Error Handling

Error Code	Condition	Action	
`0x01`	Invalid checksum	Discard packet, log error	
`0x02`	Unknown CMD_ID	Discard packet, log error	
`0x03`	Payload length mismatch	Discard packet, log error	
`0x04`	Execution failure (relay/LED)	Retry command, report via `ERROR_REPORT`	

---

Firmware Integration

ESP32 Side
- `protocol/uart_link.cpp` handles UART framing and parsing.
- Commands are dispatched via `AppCommands` to `SystemState`.
- State is authoritative; ESP32 decides action sequence.

ESP8266 Side
- `protocol/packet_parser.cpp` reads incoming UART packets.
- Queues commands for LED and relay execution.
- Mirrors canonical state received from ESP32.
- Sends acknowledgments (`STATE_ACK`) or error reports.

---

Development Notes

- Keep all UART operations non-blocking to avoid latency in other MCU tasks.
- Avoid large payloads; prefer incremental updates for LEDs or relays.
- Maintain version compatibility: future firmware updates may extend CMD_ID table.
- Use debug UART (separate from control link) for logging during development.