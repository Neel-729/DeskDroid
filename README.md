# DeskDroid
### A modular dual-MCU smart desktop ecosystem focused on automation, real-time interaction, embedded systems engineering, and scalable hardware architecture.

[![Static Badge](https://img.shields.io/badge/Version-2.6.2-green)](https://github.com/Neel-729/DeskDroid)
[![Static Badge](https://img.shields.io/badge/Previous--version-2.5-orange)](https://github.com/Neel-729/DeskDroid/blob/main/DeskDroidArchives/v2.5.main.cpp)
[![Static Badge](https://img.shields.io/badge/ESP32-Main%20Controller-blue)](#firmware-architecture)
[![Static Badge](https://img.shields.io/badge/ESP8266-Output%20Processor-lightgrey)](#firmware-architecture)

---

## What is DeskDroid?

DeskDroid is a modular embedded desktop assistant built around the ESP32 ecosystem, with the ESP8266 acting as a dedicated output processor.

The project combines:

- Smart desktop utilities
- Real-time hardware interaction
- Embedded UI systems
- Sensor and peripheral integration
- Expandable automation features
- Dual-MCU orchestration
- Future IoT capabilities

Instead of being a single-purpose gadget, DeskDroid is designed as a scalable embedded platform that can evolve into a fully integrated smart desk ecosystem.

The architecture emphasizes:

- Clean modular design
- Non-blocking firmware principles
- Hardware abstraction
- Command-driven control
- Expandability
- Maintainability
- Real-world product-oriented engineering

---

## Version 2.6.2 Notes

Version 2.6.2 reflects the current repo structure and firmware direction:

- The ESP32 firmware is the canonical application layer.
- The ESP8266 firmware is responsible for deterministic output execution.
- UART protocol sync is documented and formalized.
- System state is centralized and treated as the source of truth.
- The codebase is organized around commands, services, protocol transport, and UI rendering.
- Supporting design documentation is now split into focused markdown files.

---

## Core Features

### Current Features

- Real-time clock system
- Timer functionality
- Stopwatch functionality
- Reminder system
- NeoPixel lighting engine
- Rotary encoder navigation
- LCD-based UI system
- Audio feedback using buzzer
- Persistent settings storage
- Event-driven application flow
- Modular driver architecture
- Centralized scheduler system
- ESP32 ↔ ESP8266 link supervision
- State synchronization over UART

---

## Hardware Stack

| Component       | Purpose                                |
| --------------- | -------------------------------------- |
| ESP32 DevKit V1 | Main system controller                 |
| ESP8266         | Output processor for relays and LEDs   |
| DS1307 RTC      | Timekeeping and persistent clock       |
| 16x2 LCD (I2C)  | User interface display                 |
| Rotary Encoder  | Navigation and input                   |
| Buzzer          | Alerts and audio feedback              |
| NeoPixel LEDs   | Lighting effects and status indicators |
| 12V 20A SMPS    | Main power supply                      |

---

## Firmware Architecture

DeskDroid uses a layered modular architecture to keep the codebase scalable and maintainable.

```text
firmware/esp32/src/
├── app/        → Application state, commands, and orchestration
├── core/       → Events, scheduler, logging, settings, state
├── services/   → Lighting, timer, connectivity, protocol, settings
├── protocol/   → ESP8266 link, packet building, synchronization
├── ui/         → Screen rendering and navigation
├── features/   → User-facing feature modules
└── utils/      → Shared utilities

firmware/esp8266/src/
├── led/        → Animation runtime and LED effects
├── protocol/   → Packet parsing, dispatch, and responses
├── relay/      → Relay output control
├── state/      → Mirrored execution state
└── ...
```

### Architecture Philosophy

#### 1. Hardware Abstraction

Drivers isolate hardware-specific logic from application logic.

#### 2. Command-Driven Flow

User-facing actions pass through `AppCommands` before they reach state or hardware layers.

#### 3. Canonical State Ownership

The ESP32 owns product decisions and the canonical `SystemState`. Consumers read from state; they do not invent it.

#### 4. Dual-MCU Execution Model

The ESP32 decides what should happen. The ESP8266 applies the physical outputs, including LEDs and relays.

#### 5. Modular Expansion

New features can be added without rewriting the entire firmware.

#### 6. Scalable Design

The firmware is structured to support future peripherals, sensors, wireless modules, and automation systems.

---

## Current Modules

### Clock Module

- RTC integration
- Real-time display
- Persistent time tracking

### Timer Module

- Countdown timer support
- Audio completion alerts
- Timer alarm state handling

### Stopwatch Module

- Start / stop / reset functionality
- Live UI updates

### Reminder System

- Scheduled reminders
- Event-based notifications

### Lighting Engine

- NeoPixel animations
- System state indication
- Ambient desktop lighting
- Sync-ready execution through ESP8266

### Protocol Layer

- UART packet formatting
- Ping / pong supervision
- Full-state synchronization
- Mirror-state recovery

---

## Documentation Tags

Use these tags to jump to the supporting design docs:

- `#architecture` → [docs/architecture.md](docs/architecture.md)
- `#protocol` → [docs/protocol_spec.md](docs/protocol_spec.md)
- `#system-state` → [docs/system_state.md](docs/system_state.md)
- `#pinout` → [docs/pinout.md](docs/pinout.md)
- `#development-notes` → [docs/development_notes.md](docs/development_notes.md)
- `#v1-changelog` → [DeskDroidArchives/v1-Changelog.md](DeskDroidArchives/v1-Changelog.md)
- `#v1.1-changelog` → [DeskDroidArchives/v1.1-chnagelog.md](DeskDroidArchives/v1.1-chnagelog.md)

---

## Future Roadmap

### Planned Features

- Wi-Fi integration
- Mobile companion app
- Web dashboard
- Bluetooth connectivity expansion
- Voice interaction
- Smart relay control
- Touch sensor integration
- Multi-controller distributed architecture
- AI-assisted automation
- Sensor ecosystem support
- Advanced animation engine

### Long-Term Vision

DeskDroid is intended to evolve from a desktop utility device into a modular smart environment controller capable of handling:

- Workspace automation
- Smart peripherals
- Environmental monitoring
- Productivity workflows
- Real-time notifications
- Intelligent interaction systems

---

## Tech Stack

| Layer              | Technology         |
| ------------------ | ------------------ |
| Firmware Framework | Arduino / PlatformIO |
| Build System       | PlatformIO         |
| MCU Platform       | ESP32 + ESP8266    |
| Language           | C++                |
| Display Library    | LiquidCrystal_I2C  |
| RTC Library        | RTClib             |
| LED Library        | Adafruit NeoPixel  |

---

## Getting Started

### Requirements

- VS Code
- PlatformIO Extension
- ESP32 DevKit V1
- ESP8266 module
- USB cable

---

### Clone Repository

```bash
git clone https://github.com/Neel-729/DeskDroid.git
cd DeskDroid
```

---

### Build Firmware

Build the controller and output processor from their respective PlatformIO projects.

```bash
# ESP32 controller
cd firmware/esp32
pio run

# ESP8266 output processor
cd ../esp8266
pio run
```

---

### Upload Firmware

```bash
# ESP32 controller
cd firmware/esp32
pio run --target upload

# ESP8266 output processor
cd ../esp8266
pio run --target upload
```

---

### Serial Monitor

```bash
# ESP32 controller
cd firmware/esp32
pio device monitor
```

---

## Wiring Overview

> Pin mappings may change as development progresses.

| Peripheral     | Interface       |
| -------------- | --------------- |
| LCD            | I2C             |
| RTC            | I2C             |
| NeoPixels      | GPIO            |
| Rotary Encoder | GPIO Interrupts |
| Buzzer         | PWM GPIO        |
| ESP8266 Link   | UART            |
| Relays         | GPIO            |

---

## Engineering Goals

DeskDroid focuses heavily on real embedded engineering practices rather than quick prototyping.

Key priorities:

- Stable firmware architecture
- Maintainable codebase
- Expandable hardware ecosystem
- Non-blocking execution
- Efficient event handling
- Product-oriented system design
- Deterministic physical output control

---

## Why This Project Exists

Most DIY desk gadgets are isolated single-purpose builds.

DeskDroid aims to solve that by creating a unified platform where multiple smart systems coexist under a scalable architecture.

The project is also intended as a long-term embedded systems engineering journey involving:

- Firmware architecture
- Hardware design
- Embedded UI/UX
- Power management
- Real-time systems
- Automation engineering
- Modular product design

---

## Contributing

Contributions, suggestions, architectural feedback, and hardware ideas are welcome.

If you want to improve the firmware:

1. Fork the repository
2. Create a feature branch
3. Commit changes clearly
4. Submit a pull request

---

## License

This project is licensed under the MIT License.

See the [LICENSE](LICENSE) file for details.

---

## Author

Built by Ace.
> Neel Indalkar

Focused on embedded systems, modular engineering, automation, and scalable smart hardware systems.
