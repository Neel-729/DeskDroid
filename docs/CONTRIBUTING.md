# Contributing

## Scope and structure

Make firmware changes in `firmware/esp32` or `firmware/esp8266`. Treat `DeskDroidArchives/` and root `src/` as historical/scaffold material unless a task explicitly targets them. Keep documentation claims tied to current code and configuration.

## Architectural rules

- Keep product decisions and canonical state on the ESP32.
- Route product-facing state changes through `AppCommands` and `SystemStateStore`.
- Keep the ESP8266 focused on validated command execution, relay output, LED rendering, and runtime recovery.
- Preserve the framed UART protocol and sequence/acknowledgment behavior when changing link code.
- Do not add credentials or private hardware data to the repository.

## Before submitting a change

Run both builds:

```text
pio run -d firmware/esp32
pio run -d firmware/esp8266
```

If hardware is available, run the applicable checks in [TESTING.md](TESTING.md) and include the board/peripheral conditions in the change description. Update the relevant documentation when public behavior, pins, protocol fields, settings, versions, or build commands change.

## Documentation conventions

Use implementation names exactly as they appear in code. Distinguish verified facts from external hardware assumptions. Label historical or unimplemented behavior explicitly instead of presenting it as current.
