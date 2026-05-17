#include "buzzer_driver.h"

namespace {
constexpr uint8_t BUZZER_PIN = 4;
unsigned long beepUntil = 0;
}

namespace BuzzerDriver {

void begin(){
  pinMode(BUZZER_PIN,OUTPUT);
}

void trigger(int duration){
  digitalWrite(BUZZER_PIN,HIGH);
  beepUntil=millis()+duration;
}

void update(){
  if(digitalRead(BUZZER_PIN)==HIGH && millis()>beepUntil){
    digitalWrite(BUZZER_PIN,LOW);
  }
}

}

