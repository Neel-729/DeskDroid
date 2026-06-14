#include "navigation_manager.h"

#include "navigation_stack.h"
#include "../core/logging.h"

namespace {

// Long press detection state
enum class LongPressState : uint8_t {
  Idle,
  Monitoring,      // Button held, monitoring duration
  FeedbackActive,  // Visual feedback shown (~500ms)
  Triggered,       // Long press reached threshold
};

LongPressState longPressState = LongPressState::Idle;
unsigned long longPressStartMs = 0;
unsigned long longPressDuration = 1000;      // 1000ms default
unsigned long visualFeedbackThreshold = 500; // 500ms
AppState homeScreen = STATE_CLOCK;

}  // namespace

namespace NavigationManager {

void begin() {
  NavigationStack::begin();
  longPressState = LongPressState::Idle;
  longPressStartMs = 0;
  LOG_INFO(LogTag::APP, "[NAV-MGR] Navigation manager initialized");
}

bool update(unsigned long now) {
  // Check for long press timeout and trigger if needed
  if (longPressState == LongPressState::Monitoring ||
      longPressState == LongPressState::FeedbackActive) {
    
    unsigned long elapsedMs = now - longPressStartMs;
    
    // Transition to feedback when threshold reached
    if (longPressState == LongPressState::Monitoring &&
        elapsedMs >= visualFeedbackThreshold) {
      longPressState = LongPressState::FeedbackActive;
      LOG_INFO(LogTag::APP, "[NAV-MGR] Visual feedback should be shown");
    }
    
    // Trigger long press when duration reached
    if (elapsedMs >= longPressDuration) {
      longPressState = LongPressState::Triggered;
      LOG_INFO(LogTag::APP, "[NAV-MGR] Long press detected!");
      goHome();
      return true;
    }
  }
  
  return false;
}

void enter(AppState state) {
  // Cancel long press when transitioning screens
  longPressState = LongPressState::Idle;
  longPressStartMs = 0;
  
  // Push new state onto stack
  NavigationStack::push(state);
  LOG_INFO(LogTag::APP, "[NAV-MGR] Entered state %d", (int)state);
}

void back() {
  // Cancel long press
  longPressState = LongPressState::Idle;
  longPressStartMs = 0;
  
  // Pop stack to return to previous screen
  if (!NavigationStack::pop()) {
    LOG_INFO(LogTag::APP, "[NAV-MGR] Already at home, back is no-op");
    return;
  }
  
  LOG_INFO(LogTag::APP, "[NAV-MGR] Back pressed");
}

void goHome() {
  // Reset stack to home only
  NavigationStack::reset();
  longPressState = LongPressState::Idle;
  longPressStartMs = 0;
  LOG_INFO(LogTag::APP, "[NAV-MGR] Go home triggered");
}

bool isAtHome() {
  return NavigationStack::isAtHome();
}

AppState current() {
  return NavigationStack::peek();
}

AppState previous() {
  return NavigationStack::previous();
}

uint8_t stackDepth() {
  return NavigationStack::depth();
}

bool isLongPressInProgress() {
  return longPressState != LongPressState::Idle;
}

unsigned long longPressElapsedMs() {
  if (longPressState == LongPressState::Idle) {
    return 0;
  }
  // Return elapsed time (caller is responsible for getting current time)
  return 0;  // Caller should track this themselves
}

void reset() {
  NavigationStack::reset();
  longPressState = LongPressState::Idle;
  longPressStartMs = 0;
  LOG_INFO(LogTag::APP, "[NAV-MGR] Navigation reset");
}

void setHomeScreen(AppState homeState) {
  homeScreen = homeState;
  LOG_INFO(LogTag::APP, "[NAV-MGR] Home screen set to %d", (int)homeState);
}

void setLongPressDuration(unsigned long durationMs) {
  longPressDuration = durationMs;
  LOG_INFO(LogTag::APP, "[NAV-MGR] Long press duration set to %ldms", durationMs);
}

void setVisualFeedbackThreshold(unsigned long thresholdMs) {
  visualFeedbackThreshold = thresholdMs;
  LOG_INFO(LogTag::APP, "[NAV-MGR] Visual feedback threshold set to %ldms", thresholdMs);
}

bool shouldShowVisualFeedback() {
  return longPressState == LongPressState::FeedbackActive;
}

void debugPrint() {
  Serial.printf("[NAV-MGR] Navigation Manager State:\n");
  Serial.printf("  Home Screen: %d\n", (int)homeScreen);
  Serial.printf("  Current: %d\n", (int)current());
  Serial.printf("  Stack Depth: %d\n", stackDepth());
  Serial.printf("  Long Press State: %d\n", (int)longPressState);
  NavigationStack::debugPrint();
}

}  // namespace NavigationManager
