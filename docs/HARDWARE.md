# Hardware

## Verified controller hardware

The ESP32 PlatformIO environment targets `esp32doit-devkit-v1`; the ESP8266 environment targets `nodemcuv2`. The code directly uses:

- DS1307 RTC through RTClib on the shared ESP32 I2C bus.
- A 16x2 LCD at I2C address `0x27` on the same bus. The driver configures SDA GPIO21, SCL GPIO22, 400 kHz, and a 50 ms Wire timeout.
- A quadrature rotary encoder on GPIO19 (CLK), GPIO18 (DT), and GPIO5 (switch). The inputs use internal pull-ups; the switch is considered pressed when low.
- A buzzer on ESP32 GPIO23. The driver drives it high for a requested pulse and turns it off from the 5 ms scheduler task.
- A TTP229-BSF touch controller on GPIO32 (SCL) and GPIO35 (SDO). The live code clocks 16 bits, assumes active-low keys, and logs transitions as `[MACRO] KEY n PRESSED/RELEASED`; it does not enqueue application events.
- A 100-pixel NeoPixel strip on ESP8266 GPIO2. The ESP8266 uses `NEO_GRB + NEO_KHZ800` and renders at a 10 ms frame interval.
- Four active-low relay outputs on ESP8266 GPIO5, GPIO4, GPIO14, and GPIO12.

The ESP32 source also contains a NeoPixel driver configured for GPIO13, but the live application does not initialize or call it. Current LED output therefore requires the ESP8266 processor.

## Inter-board connection

Connect ESP32 GPIO17 (TX) to ESP8266 RX GPIO3, ESP32 GPIO16 (RX) to ESP8266 TX GPIO1, and a common ground. Both firmware projects configure 115200 baud, 8N1. The repository verifies the signal names and baud rate, but does not verify voltage compatibility, level shifting, cable length, termination, or power design.

## LED and relay behavior

The ESP8266 initializes relays inactive, then applies the mirrored relay state after a valid `FULL_SYNC`. NeoPixel power is represented by `PWR`; `PWR=0` clears the strip even if an effect is selected. Effects are `NONE`, `SOLID`, `BREATHING`, `RAINBOW`, and `AMBIENT`.

## Hardware not established by this repository

The source does not provide a complete schematic, connector pinout, supply/current budget, relay contact ratings, electrical isolation requirements, or a verified 3.3 V/5 V interface design. The old wiring note mentions a 12 V supply, but the firmware does not define or validate that supply; treat it as an external integration detail requiring independent verification.
