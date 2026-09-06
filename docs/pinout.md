# Pinout

This is the compact pin table. [HARDWARE.md](HARDWARE.md) contains interface details and limits of what the repository verifies.

## ESP32 controller

| GPIO | Function |
| ---: | --- |
| 5 | Encoder switch, active low, internal pull-up |
| 18 | Encoder DT, internal pull-up |
| 19 | Encoder CLK, internal pull-up |
| 16 | UART RX from ESP8266 |
| 17 | UART TX to ESP8266 |
| 21 | I2C SDA for LCD and DS1307 |
| 22 | I2C SCL for LCD and DS1307 |
| 23 | Buzzer output |
| 32 | TTP229-BSF SCL |
| 35 | TTP229-BSF SDO input |
| 13 | Defined for the unused ESP32 NeoPixel driver; not used by the live application |

## ESP8266 output processor

| GPIO | Function |
| ---: | --- |
| 1 | UART TX |
| 3 | UART RX |
| 2 | NeoPixel data |
| 5 | Relay 1 |
| 4 | Relay 2 |
| 14 | Relay 3 |
| 12 | Relay 4 |

All four relay outputs are active low in `firmware/esp8266/include/config.h`.
