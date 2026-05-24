#pragma once

#include <Arduino.h>

namespace Pins {

constexpr uint8_t UartTx = 1;
constexpr uint8_t UartRx = 3;

constexpr uint8_t NeoPixelData = 2;

constexpr uint8_t Relay1 = 5;
constexpr uint8_t Relay2 = 4;
constexpr uint8_t Relay3 = 14;
constexpr uint8_t Relay4 = 12;
constexpr uint8_t RelayPins[] = {Relay1, Relay2, Relay3, Relay4};

}  // namespace Pins

