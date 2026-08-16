#include "neopixel_driver.h"
#include "../../include/config.h"
#include "../../include/pins.h"

#include <Adafruit_NeoPixel.h>

namespace {
Adafruit_NeoPixel pixels(Config::NeoPixelCount, Pins::NeoPixelData, NEO_GRB + NEO_KHZ800);
LedState currentLedState = LED_IDLE;
LedIdlePreset idlePreset = IDLE_STATIC;
uint8_t ledBrightness = 5;
}

namespace NeoPixelDriver {

void begin(uint8_t brightnessLevel, uint8_t preset){
  ledBrightness = brightnessLevel;
  if(ledBrightness > 10) ledBrightness = 10;

  pixels.begin();
  pixels.setBrightness(ledBrightness * 25.5);
  pixels.clear();

  if(preset > IDLE_PULSE) preset = IDLE_STATIC;
  idlePreset = (LedIdlePreset)preset;
  pixels.show();
}

void setState(LedState state){
  currentLedState = state;
}

void setIdlePreset(LedIdlePreset preset){
  idlePreset = preset;
}

void setBrightnessLevel(uint8_t level){
  ledBrightness = level;
  if(ledBrightness > 10) ledBrightness = 10;
  pixels.setBrightness(ledBrightness * 25.5);
  pixels.show();
}

void update(bool lightsAllowed){
  static bool toggle = false;

  toggle = !toggle;

  switch(currentLedState){
    case LED_IDLE:
      if(!lightsAllowed){
        pixels.clear();
        break;
      }

      switch(idlePreset){
        case IDLE_OFF:
          pixels.clear();
          break;

        case IDLE_STATIC:
          pixels.fill(pixels.Color(30,0,20));
          break;

        case IDLE_BREATH: {
          static float phase = 0.0;
          static uint16_t baseHue = 0;

          phase += 0.06;
          if(phase > TWO_PI){
            phase -= TWO_PI;
            baseHue += 1800;
          }

          float wave = (sin(phase) + 1.0) * 0.5;
          float minLevel = 0.15;
          float adjusted = minLevel + (1.0 - minLevel) * wave;
          float corrected = pow(adjusted, 2.2);
          uint8_t brightness = corrected * 255;
          uint32_t color = pixels.ColorHSV(baseHue);

          uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
          uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
          uint8_t b = (color & 0xFF) * brightness / 255;

          pixels.fill(pixels.Color(r, g, b));
          break;
        }

        case IDLE_RAINBOW: {
          static uint16_t hue = 0;
          hue += 256;

          for(int i=0;i<Config::NeoPixelCount;i++){
            pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV(hue + (i * 65536L / Config::NeoPixelCount))));
          }
          break;
        }

        case IDLE_PULSE: {
          static float phase = 0.0;
          phase += 0.05;
          float wave = (sin(phase) + 1.0) * 0.5;
          float corrected = pow(wave, 2.2);
          uint8_t level = corrected * ledBrightness * 25.5;
          uint8_t r = level / 4;
          uint8_t g = 0;
          uint8_t b = level / 6;
          pixels.fill(pixels.Color(r, g, b));
          break;
        }
      }
      break;

    case LED_TIMER_ALARM:
      if(toggle) pixels.fill(pixels.Color(255,0,0));
      else pixels.clear();
      break;

    case LED_REMINDER_ALARM:
      if(toggle) pixels.fill(pixels.Color(255,80,0));
      else pixels.clear();
      break;

    case LED_SUCCESS:
      pixels.fill(pixels.Color(0,255,0));
      break;
  }

  pixels.show();
}

}