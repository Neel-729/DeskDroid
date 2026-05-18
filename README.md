# DeskDroid
### A modular ESP32-powered smart desktop ecosystem focused on automation, real-time interaction, embedded systems engineering, and scalable hardware architecture.

[![Static Badge](https://img.shields.io/badge/Version-2.6-green)](https://github.com/Neel-729/DeskDroid)
[![Static Badge](https://img.shields.io/badge/Previous--version-2.5-orange)](https://github.com/Neel-729/DeskDroid/blob/main/DeskDroidArchives/v2.5.main.cpp)


---

## What is DeskDroid?

DeskDroid is a modular embedded desktop assistant built around the ESP32 ecosystem.

The project combines:

- Smart desktop utilities
- Real-time hardware interaction
- Embedded UI systems
- Sensor integration
- Expandable automation features
- Future IoT capabilities

Instead of being a single-purpose gadget, DeskDroid is designed as a scalable embedded platform that can evolve into a fully integrated smart desk ecosystem.

The architecture emphasizes:

- Clean modular design
- Non-blocking firmware principles
- Hardware abstraction
- Expandability
- Maintainability
- Real-world product-oriented engineering

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

---

## Hardware Stack

| Component       | Purpose                                |
| --------------- | -------------------------------------- |
| ESP32 DevKit V1 | Main system controller                 |
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
src/
├── app/        → Application state and orchestration
├── core/       → Events, scheduler, logging, settings
├── drivers/    → Hardware abstraction layer
├── features/   → User-facing features and modules
├── ui/         → Screen rendering and navigation
├── utils/      → Shared utilities
└── main.cpp    → System entry point
```

### Architecture Philosophy

#### 1. Hardware Abstraction

Drivers isolate hardware-specific logic from application logic.

#### 2. Event-Driven Flow

The system avoids tightly coupled logic by routing interactions through centralized event handling.

#### 3. Modular Expansion

New features can be added without rewriting the entire firmware.

#### 4. Scalable Design

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
| Firmware Framework | Arduino            |
| Build System       | PlatformIO         |
| MCU Platform       | ESP32              |
| Language           | C++                |
| Display Library    | LiquidCrystal\_I2C |
| RTC Library        | RTClib             |
| LED Library        | Adafruit NeoPixel  |

---

## Getting Started

### Requirements

- VS Code
- PlatformIO Extension
- ESP32 DevKit V1
- USB cable

---

### Clone Repository

```bash
git clone https://github.com/Neel-729/DeskDroid.git
cd DeskDroid
```

---

### Build Firmware

```bash
pio run
```

---

### Upload Firmware

```bash
pio run --target upload
```

---

### Serial Monitor

```bash
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

