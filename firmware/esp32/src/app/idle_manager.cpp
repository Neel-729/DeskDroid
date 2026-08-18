#include "idle_manager.h"

#include "../core/logging.h"

namespace {

// Idle state tracking
unsigned long lastActivityTime = 0;
IdleManager::IdleTimeout currentTimeout = IdleManager::IdleTimeout::THIRTY_SECONDS;
bool idleReturnBlocked = false;
bool timeoutFired = false;  // Latch flag to ensure timeout is only signaled/logged once
bool isTimerActive = false; // Whether timer is armed/active

// Visual feedback timing (show countdown 3 seconds before return)
constexpr unsigned long FEEDBACK_LEAD_TIME_MS = 3000;

}  // namespace

namespace IdleManager {

void begin() {
  // Initialize with current time
  lastActivityTime = millis();
  currentTimeout = IdleTimeout::THIRTY_SECONDS;
  idleReturnBlocked = false;
  timeoutFired = false;  // Reset latch on initialization
  isTimerActive = false; // Start disarmed since we begin at HOME
  LOG_INFO(LogTag::APP, "[IDLE] Manager initialized, timeout=%u seconds, timer disarmed", 
           (uint16_t)currentTimeout);
}

bool update(unsigned long now) {
  // If timer is not active, idle return is blocked, or timeout is disabled, never trigger auto-return
  if (!isTimerActive || idleReturnBlocked || currentTimeout == IdleTimeout::OFF) {
    return false;
  }
  
  uint16_t timeoutMs = (uint16_t)currentTimeout * 1000;
  unsigned long idleDurationMs = now - lastActivityTime;
  
  // Only signal/log timeout once per inactivity period (latch behavior)
  if (idleDurationMs >= timeoutMs && !timeoutFired) {
    timeoutFired = true;  // Latch the flag to prevent repeated calls
    LOG_INFO(LogTag::APP, "[IDLE] Idle timeout reached (%lums >= %ums)", 
             idleDurationMs, timeoutMs);
    return true;  // Signal auto-return exactly once
  }
  
  return false;
}

void notifyActivity(unsigned long now) {
  lastActivityTime = now;
  timeoutFired = false;  // Reset latch when user activity is detected
}

bool isIdle(unsigned long now) {
  if (!isTimerActive || currentTimeout == IdleTimeout::OFF) {
    return false;
  }
  
  uint16_t timeoutMs = (uint16_t)currentTimeout * 1000;
  unsigned long idleDurationMs = now - lastActivityTime;
  
  return idleDurationMs >= timeoutMs;
}

bool isIdleReturnCountdown(unsigned long now) {
  if (!isTimerActive || idleReturnBlocked || currentTimeout == IdleTimeout::OFF) {
    return false;
  }
  
  uint16_t timeoutMs = (uint16_t)currentTimeout * 1000;
  unsigned long idleDurationMs = now - lastActivityTime;
  
  // Return true if we're in the countdown phase (last 3 seconds before timeout)
  return (idleDurationMs >= (timeoutMs - FEEDBACK_LEAD_TIME_MS)) &&
         (idleDurationMs < timeoutMs);
}

uint8_t secondsUntilIdleReturn(unsigned long now) {
  if (!isTimerActive || idleReturnBlocked || currentTimeout == IdleTimeout::OFF) {
    return 0;
  }
  
  uint16_t timeoutMs = (uint16_t)currentTimeout * 1000;
  unsigned long idleDurationMs = now - lastActivityTime;
  
  if (idleDurationMs >= timeoutMs) {
    return 0;  // Already at or past timeout
  }
  
  unsigned long remainingMs = timeoutMs - idleDurationMs;
  return (uint8_t)(remainingMs / 1000);
}

void setIdleTimeout(IdleTimeout timeout) {
  currentTimeout = timeout;
  LOG_INFO(LogTag::APP, "[IDLE] Timeout set to %u seconds", (uint16_t)timeout);
}

IdleTimeout getIdleTimeout() {
  return currentTimeout;
}

uint16_t getIdleTimeoutSeconds() {
  return (uint16_t)currentTimeout;
}

unsigned long getLastActivityTime() {
  return lastActivityTime;
}

unsigned long getIdleDurationMs(unsigned long now) {
  return now - lastActivityTime;
}

void blockIdleReturn(bool blocked) {
  idleReturnBlocked = blocked;
  if (blocked) {
    LOG_INFO(LogTag::APP, "[IDLE] Auto-return blocked");
  } else {
    LOG_INFO(LogTag::APP, "[IDLE] Auto-return unblocked");
  }
}

bool isIdleReturnBlocked() {
  return idleReturnBlocked;
}

void setActive(bool active) {
  if (isTimerActive != active) {
    isTimerActive = active;
    if (active) {
      // When arming the timer, reset the last activity time and clear the timeout latch
      lastActivityTime = millis();
      timeoutFired = false;
      LOG_INFO(LogTag::APP, "[IDLE] Timer armed");
    } else {
      // When disarming, clear the timeout latch to prevent stale state
      timeoutFired = false;
      LOG_INFO(LogTag::APP, "[IDLE] Timer disarmed");
    }
  }
}

bool isActive() {
  return isTimerActive;
}

void debugPrint(unsigned long now) {
  unsigned long idleDurationMs = getIdleDurationMs(now);
  uint8_t secondsRemaining = secondsUntilIdleReturn(now);
  
  Serial.printf("[IDLE] State Report:\n");
  Serial.printf("  Timeout: %u seconds\n", getIdleTimeoutSeconds());
  Serial.printf("  Idle Duration: %lu ms\n", idleDurationMs);
  Serial.printf("  Blocked: %s\n", isIdleReturnBlocked() ? "YES" : "NO");
  Serial.printf("  In Countdown: %s\n", isIdleReturnCountdown(now) ? "YES" : "NO");
  Serial.printf("  Seconds Until Return: %u\n", secondsRemaining);
}

}  // namespace IdleManager