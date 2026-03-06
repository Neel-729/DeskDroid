# DeskDroid

DeskDroid is an ESP32-powered smart desk companion device featuring a clock, timer, stopwatch, and productivity tools.

## Features

- Real Time Clock (DS1307)
- Timer with editing interface
- Stopwatch
- Rotary encoder UI
- Audible alerts
- Scrolling philosophy quotes
- Non-blocking firmware architecture

## Hardware

- ESP32 DevKit V1
- DS1307 RTC module
- 16x2 I2C LCD
- Rotary encoder
- Buzzer

## Pin Configuration

| Component    | Pin |
|--------------|-----|
| Encoder CLK  | 32  |
| Encoder DT   | 33  |
| Encoder SW   | 25  |
| Buzzer       | 26  |
| I2C SDA      | 21  |
| I2C SCL      | 22  |

## Firmware Architecture

- Event-driven input system
- Non-blocking design using millis()
- State-machine UI

## Planned Features (V2)

- Reminder system
- Settings menu
- Etc

## License

MIT