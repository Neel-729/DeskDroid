#include "encoder_driver.h"

#include <Arduino.h>

namespace {
constexpr uint8_t ENCODER_CLK = 19;
constexpr uint8_t ENCODER_DT  = 18;
constexpr uint8_t ENCODER_SW  = 5;

constexpr uint16_t LONG_PRESS = 600;
constexpr uint16_t DOUBLE_PRESS_TIME = 300;

bool buttonDown=false;
unsigned long buttonDownTime=0;

bool singlePressWaiting=false;
unsigned long singlePressTime=0;

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
}

namespace EncoderDriver {

void begin(){
  pinMode(ENCODER_CLK,INPUT_PULLUP);
  pinMode(ENCODER_DT,INPUT_PULLUP);
  pinMode(ENCODER_SW,INPUT_PULLUP);
}

EventType readEvent(){
  int movement=readEncoder();
  if(movement>0) return EVENT_ROTATE_CW;
  if(movement<0) return EVENT_ROTATE_CCW;

  bool pressed = digitalRead(ENCODER_SW)==LOW;
  unsigned long now = millis();

  if(pressed && !buttonDown){
    buttonDown=true;
    buttonDownTime=now;
  }

  if(!pressed && buttonDown){
    unsigned long duration=now-buttonDownTime;
    buttonDown=false;
    if(duration>LONG_PRESS) return EVENT_LONG_PRESS;

    if(singlePressWaiting && (now-singlePressTime<DOUBLE_PRESS_TIME)){
      singlePressWaiting=false;
      return EVENT_DOUBLE_CLICK;
    }
    singlePressWaiting=true;
    singlePressTime=now;
  }

  if(singlePressWaiting && (now-singlePressTime>DOUBLE_PRESS_TIME)){
    singlePressWaiting=false;
    return EVENT_CLICK;
  }

  return EVENT_NONE;
}

}

