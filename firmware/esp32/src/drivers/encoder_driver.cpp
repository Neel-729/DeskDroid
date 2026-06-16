#include "encoder_driver.h"

#include <Arduino.h>

namespace {
constexpr uint8_t ENCODER_CLK = 19;
constexpr uint8_t ENCODER_DT  = 18;
constexpr uint8_t ENCODER_SW  = 5;

constexpr uint16_t BUTTON_DEBOUNCE = 25;      // Reduced debounce for faster response
constexpr uint16_t LONG_PRESS_HOME = 1000;   // 1000ms for home long press (matches original requirement)
constexpr uint16_t DOUBLE_CLICK_WINDOW = 300; // Max time between clicks for double-click

// Button state tracking
bool buttonDown = false;
unsigned long buttonDownTime = 0;
bool pendingSingleClick = false;
unsigned long singleClickTime = 0;

// Raw and debounced state tracking
bool lastRawButtonState = false;
bool debouncedButtonState = false;
unsigned long lastButtonChangeTime = 0;

bool switchPressed(){
  return digitalRead(ENCODER_SW)==LOW;
}

int8_t readEncoder(){
  static uint8_t prevState=0;
  static int8_t stepAccum=0;

  prevState<<=2;

  if(digitalRead(ENCODER_CLK)) prevState|=0x02;
  if(digitalRead(ENCODER_DT))  prevState|=0x01;

  prevState&=0x0F;

  if(prevState==0b1101||prevState==0b0100||prevState==0b0010||prevState==0b1011) stepAccum++;
  if(prevState==0b1110||prevState==0b0111||prevState==0b0001||prevState==0b1000) stepAccum--;

  if(stepAccum>=2){ stepAccum=0; return 1; }
  if(stepAccum<=-2){ stepAccum=0; return -1; }

  return 0;
}

EventType readButtonEvent(){
  unsigned long now = millis();
  bool rawPressed = switchPressed();

  // Update debouncing
  if(rawPressed != lastRawButtonState){
    lastRawButtonState = rawPressed;
    lastButtonChangeTime = now;
  }

  // Apply debounce
  bool pressed = debouncedButtonState;
  if(rawPressed != debouncedButtonState && (now - lastButtonChangeTime) >= BUTTON_DEBOUNCE){
    debouncedButtonState = rawPressed;
    pressed = debouncedButtonState;
  }

  // Button just pressed
  if(pressed && !buttonDown){
    buttonDown = true;
    buttonDownTime = now;
    
    // If we get a new press while a single click is pending, it's a double click!
    if(pendingSingleClick){
      pendingSingleClick = false;
      return EVENT_DOUBLE_CLICK;
    }
  }

  // Button released
  if(!pressed && buttonDown){
    unsigned long pressDuration = now - buttonDownTime;
    buttonDown = false;
    
    // Check for long press home
    if(pressDuration >= LONG_PRESS_HOME){
      return EVENT_LONG_PRESS;
    }
    
    // Normal click - start waiting for double click
    pendingSingleClick = true;
    singleClickTime = now;
    // Emit click immediately for instant feedback!
    return EVENT_CLICK;
  }

  // Check if pending single click has expired (no double click came)
  if(pendingSingleClick && (now - singleClickTime) > DOUBLE_CLICK_WINDOW){
    pendingSingleClick = false;
  }

  return EVENT_NONE;
}
}

namespace EncoderDriver {

void begin(){
  pinMode(ENCODER_CLK,INPUT_PULLUP);
  pinMode(ENCODER_DT,INPUT_PULLUP);
  pinMode(ENCODER_SW,INPUT_PULLUP);

  lastRawButtonState=switchPressed();
  debouncedButtonState=lastRawButtonState;
  lastButtonChangeTime=millis();
  buttonDown=debouncedButtonState;
  buttonDownTime=buttonDown ? lastButtonChangeTime : 0;
  singlePressWaiting=false;
}

EventType readEvent(){
  int movement=readEncoder();

  EventType buttonEvent=readButtonEvent();
  if(buttonEvent!=EVENT_NONE) return buttonEvent;

  if(movement>0) return EVENT_ROTATE_CW;
  if(movement<0) return EVENT_ROTATE_CCW;

  return EVENT_NONE;
}

}