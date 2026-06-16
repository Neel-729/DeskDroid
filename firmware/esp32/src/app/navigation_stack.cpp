#include "navigation_stack.h"

#include "../core/logging.h"

namespace {

// Maximum stack depth (navigation layers)
constexpr uint8_t MAX_STACK_DEPTH = 8;

// Stack storage
AppState stack[MAX_STACK_DEPTH] = {};
uint8_t stackDepth = 0;

}  // namespace

namespace NavigationStack {

void begin() {
  // Initialize with HOME screen
  stack[0] = STATE_CLOCK;
  stackDepth = 1;
  LOG_INFO(LogTag::APP, "[NAV] Stack initialized with HOME");
}

bool push(AppState state) {
  if (stackDepth >= MAX_STACK_DEPTH) {
    // Temporary instrumentation for root-cause validation
    Serial.printf("[NAV ERROR] stack full depth=%d state=%d\n", stackDepth, (int)state);
    LOG_WARN(LogTag::APP, "[NAV] Stack full, cannot push state");
    return false;
  }
  
  // Temporary instrumentation for root-cause validation
  Serial.printf("[NAV PUSH] state=%d depth_before=%d\n", (int)state, stackDepth);
  
  // Avoid pushing the same state twice consecutively
  if (stackDepth > 0 && stack[stackDepth - 1] == state) {
    return true;
  }
  
  stack[stackDepth] = state;
  stackDepth++;
  LOG_INFO(LogTag::APP, "[NAV] Push state, depth now %d", stackDepth);
  return true;
}

bool pop() {
  // Never pop HOME - maintain at least one state
  if (stackDepth <= 1) {
    LOG_INFO(LogTag::APP, "[NAV] At HOME, cannot pop");
    return false;
  }
  
  // Temporary instrumentation for root-cause validation
  Serial.printf("[NAV POP] depth_before=%d\n", stackDepth);
  
  stackDepth--;
  LOG_INFO(LogTag::APP, "[NAV] Pop state, depth now %d", stackDepth);
  return true;
}

AppState peek() {
  if (stackDepth == 0) {
    LOG_WARN(LogTag::APP, "[NAV] Stack empty!");
    return STATE_CLOCK;  // Default to HOME
  }
  return stack[stackDepth - 1];
}

AppState peekAt(uint8_t depth) {
  if (depth >= stackDepth) {
    return STATE_CLOCK;  // Return HOME if depth out of bounds
  }
  return stack[stackDepth - 1 - depth];
}

AppState previous() {
  if (stackDepth < 2) {
    return STATE_CLOCK;  // No previous screen, return HOME
  }
  return stack[stackDepth - 2];
}

bool isAtHome() {
  return stackDepth <= 1 || peek() == STATE_CLOCK;
}

uint8_t depth() {
  return stackDepth;
}

uint8_t maxDepth() {
  return MAX_STACK_DEPTH;
}

void reset() {
  // Reset to home only
  stack[0] = STATE_CLOCK;
  stackDepth = 1;
  LOG_INFO(LogTag::APP, "[NAV] Stack reset to HOME");
}

void replace(AppState state) {
  if (stackDepth == 0) {
    push(state);
    return;
  }
  stack[stackDepth - 1] = state;
  LOG_INFO(LogTag::APP, "[NAV] Replaced top of stack, depth %d", stackDepth);
}

void debugPrint() {
  Serial.printf("[NAV] Stack depth: %d\n", stackDepth);
  for (uint8_t i = 0; i < stackDepth; i++) {
    AppState state = stack[i];
    Serial.printf("  [%d] State: %d\n", i, (int)state);
  }
}

}  // namespace NavigationStack