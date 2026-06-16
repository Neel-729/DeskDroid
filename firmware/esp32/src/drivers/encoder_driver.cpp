#include "encoder_driver.h"

// Compile-time debug toggle for input FSM logging (disabled by default for production)
#ifndef INPUT_DEBUG
#define INPUT_DEBUG 0
#endif
#if INPUT_DEBUG
#define INPUT_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define INPUT_LOG(...)
#endif

#include <Arduino.h>

namespace {
constexpr uint8_t ENCODER_CLK = 19;
constexpr uint8_t ENCODER_DT  = 18;
constexpr uint8_t ENCODER_SW  = 5;

constexpr uint16_t BUTTON_DEBOUNCE = 25;      // Reduced debounce for faster response
constexpr uint16_t LONG_PRESS_HOME = 1000;   // 1000ms for home long press (matches original requirement)
constexpr uint16_t DOUBLE_CLICK_ARBITRATION = 180; // 180ms arbitration window for double-click (user-friendly range)
constexpr uint16_t DOUBLE_CLICK_MAX_GAP = 350;     // Max time between first and second click release

// Button state tracking - all gesture state variables in one place for full auditability
enum class ButtonGestureState {
  IDLE,                // Ready for new input
  BUTTON_PRESSED,      // Button pressed but not released yet
  WAITING_SECOND_CLICK, // First click released, waiting for potential second click
  CONSUMED_UNTIL_RELEASE // Double-click consumed, block processing until button release
};
ButtonGestureState gestureState = ButtonGestureState::IDLE;

// State variables - all reset to baseline in resetGestureState()
bool buttonDown = false;
unsigned long buttonDownTime = 0;
unsigned long firstClickReleaseTime = 0;

// Debounce state tracking (static, never modified outside debounce logic)
static bool lastRawButtonState = false;
static bool debouncedButtonState = false;
static unsigned long lastButtonChangeTime = 0;

// Single point of reset for ALL gesture state - prevents stale state leakage
void resetGestureState() {
  gestureState = ButtonGestureState::IDLE;
  buttonDown = false;
  buttonDownTime = 0;
  firstClickReleaseTime = 0;
}

// Always ensure we return to IDLE after any gesture emission (only for single/long press)
EventType emitGestureEvent(EventType event) {
  // STAGE 1: Log event generation from encoder driver (only when INPUT_DEBUG is enabled)
  INPUT_LOG("[EVENT] %d\n", event);
  resetGestureState();
  return event;
}

// ISR-safe atomic state for edge capture - only raw GPIO transitions
volatile uint8_t lastEncoderState = 0;       // Last captured CLK/DT state (bits 1=CLK, 0=DT)
volatile int8_t encoderStepAccum = 0;       // Atomic step accumulator, only modified in ISR
volatile unsigned long lastButtonEdgeTime = 0; // Timestamp of last button edge (us)
volatile bool lastButtonRawState = false;     // Last raw button state captured in ISR

// IRAM-safe ISR for GPIO edge capture (only minimal operations, compliant with ESP32 best practices)
void IRAM_ATTR encoderISR() {
  // Use safe digitalRead() inside ISR for initial validation (per requirements)
  bool clk = digitalRead(ENCODER_CLK);
  bool dt = digitalRead(ENCODER_DT);
  bool sw = digitalRead(ENCODER_SW);

  // Process encoder quadrature
  uint8_t newState = (clk << 1) | dt;
  if(newState != lastEncoderState) {
    // Quadrature transition detection - same logic as original polling implementation
    uint8_t combined = (lastEncoderState << 2) | newState;
    if(combined == 0b1101 || combined == 0b0100 || combined == 0b0010 || combined == 0b1011) {
      encoderStepAccum++;
    } else if(combined == 0b1110 || combined == 0b0111 || combined == 0b0001 || combined == 0b1000) {
      encoderStepAccum--;
    }
    lastEncoderState = newState;
  }

  // Process button edge capture only - timestamp changes, no debounce
  if(sw != lastButtonRawState) {
    lastButtonRawState = sw;
    lastButtonEdgeTime = micros(); // Only capture timestamp of raw edge
  }
}

bool switchPressed(){
  return digitalRead(ENCODER_SW)==LOW;
}

int8_t readEncoder(){
  // Original polling fallback (will be removed after interrupt validation, but safe to keep)
  static uint8_t pollPrevState=0;
  static int8_t pollStepAccum=0;

  pollPrevState<<=2;
  if(digitalRead(ENCODER_CLK)) pollPrevState|=0x02;
  if(digitalRead(ENCODER_DT))  pollPrevState|=0x01;
  pollPrevState&=0x0F;

  if(pollPrevState==0b1101||pollPrevState==0b0100||pollPrevState==0b0010||pollPrevState==0b1011) pollStepAccum++;
  if(pollPrevState==0b1110||pollPrevState==0b0111||pollPrevState==0b0001||pollPrevState==0b1000) pollStepAccum--;

  // Also consume ISR accumulated steps in critical section
  int8_t totalSteps = 0;
  noInterrupts();
  if(encoderStepAccum >= 2) {
    totalSteps += 1;
    encoderStepAccum = 0;
  } else if(encoderStepAccum <= -2) {
    totalSteps += -1;
    encoderStepAccum = 0;
  }
  interrupts();

  // Process polling accumulated steps
  if(pollStepAccum>=2){ pollStepAccum=0; totalSteps +=1; }
  if(pollStepAccum<=-2){ pollStepAccum=0; totalSteps +=-1; }

  return totalSteps;
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

  // Log FSM state for validation (only when INPUT_DEBUG is enabled)
  INPUT_LOG("[FSM] state=%d pressed=%d buttonDown=%d firstClick=%lu\n", (int)gestureState, pressed, buttonDown, firstClickReleaseTime);

  // Handle CONSUMED_UNTIL_RELEASE state - block all processing until button is released
  if(gestureState == ButtonGestureState::CONSUMED_UNTIL_RELEASE){
    if(!pressed){
      // Button released, reset to IDLE and resume normal operation
      resetGestureState();
      INPUT_LOG("[FSM] Transition CONSUMED_UNTIL_RELEASE->IDLE\n");
    }
    return EVENT_NONE;
  }

  // Handle IDLE state - only process new button press
  if(gestureState == ButtonGestureState::IDLE){
    if(pressed && !buttonDown){
      // First button press - transition to pressed state
      buttonDown = true;
      buttonDownTime = now;
      gestureState = ButtonGestureState::BUTTON_PRESSED;
    }
    return EVENT_NONE;
  }

  // Handle BUTTON_PRESSED state - process release or long press
  if(gestureState == ButtonGestureState::BUTTON_PRESSED){
    if(pressed){
      // Still pressed - check for long press
      unsigned long pressDuration = now - buttonDownTime;
      if(pressDuration >= LONG_PRESS_HOME){
        return emitGestureEvent(EVENT_LONG_PRESS);
      }
      return EVENT_NONE;
    } else {
      // Button released - transition to wait for second click
      buttonDown = false;
      firstClickReleaseTime = now;
      gestureState = ButtonGestureState::WAITING_SECOND_CLICK;
      return EVENT_NONE;
    }
  }

  // Handle WAITING_SECOND_CLICK state - check for second click or timeout
  if(gestureState == ButtonGestureState::WAITING_SECOND_CLICK){
    // Check for second button press FIRST (per original order)
    if(pressed && !buttonDown){
      // Second click detected - emit double click immediately, transition to CONSUMED_UNTIL_RELEASE
      buttonDown = true;
      buttonDownTime = now;
      Serial.printf("[EVENT] %d\n", EVENT_DOUBLE_CLICK);
      INPUT_LOG("[FSM] Transition WAITING_SECOND_CLICK->CONSUMED_UNTIL_RELEASE\n");
      gestureState = ButtonGestureState::CONSUMED_UNTIL_RELEASE;
      return EVENT_DOUBLE_CLICK;
    }
    
    // Check if arbitration window expired - emit single click
    if(now - firstClickReleaseTime > DOUBLE_CLICK_ARBITRATION){
      return emitGestureEvent(EVENT_CLICK);
    }
    
    // Still waiting - no event yet
    return EVENT_NONE;
  }

  // Safety fallback - should never reach here, but reset if we do
  resetGestureState();
  return EVENT_NONE;
}
}

namespace EncoderDriver {

void begin(){
  pinMode(ENCODER_CLK,INPUT_PULLUP);
  pinMode(ENCODER_DT,INPUT_PULLUP);
  pinMode(ENCODER_SW,INPUT_PULLUP);

  // Initialize ISR state first
  lastEncoderState = (digitalRead(ENCODER_CLK) << 1) | digitalRead(ENCODER_DT);
  lastButtonRawState = digitalRead(ENCODER_SW);
  lastButtonEdgeTime = micros();
  
  // Initialize debounce variables
  lastRawButtonState=switchPressed();
  debouncedButtonState=lastRawButtonState;
  lastButtonChangeTime=millis();
  
  // Reset all gesture state to ensure clean startup
  resetGestureState();

  // Attach interrupts for both encoder pins to capture all edges (FALLING + RISING)
  attachInterrupt(ENCODER_CLK, encoderISR, CHANGE);
  attachInterrupt(ENCODER_DT, encoderISR, CHANGE);
  attachInterrupt(ENCODER_SW, encoderISR, CHANGE);
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