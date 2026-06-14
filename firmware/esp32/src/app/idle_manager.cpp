#include "idle_manager.h"

#include "../core/logging.h"

namespace {

// Idle state tracking
unsigned long lastActivityTime = 0;
IdleManager::IdleTimeout currentTimeout = IdleManager::IdleTimeout::THIRTY_SECONDS;
bool idleReturnBlocked = false;

// Visual feedback timing (show countdown 3 seconds before return)
constexpr unsigned long FEEDBACK_LEAD_TIME_MS = 3000;

}  // namespace

namespace IdleManager {

void begin() {
  // Initialize with current time
  lastActivityTime = millis();
  currentTimeout = IdleTimeout::THIRTY_SECONDS;
  idleReturnBlocked = false;
  LOG_INFO(LogTag::APP, "[IDLE] Manager initialized, timeout=%u seconds", 
           (uint16_t)currentTimeout);
}

bool update(unsigned long now) {
  // If idle return is blocked, never trigger auto-return
  if (idleReturnBlocked) {
    return false;
  }
  
  // If timeout is disabled, never trigger auto-return
  if (currentTimeout == IdleTimeout::OFF) {
    return false;
  }
  
  uint16_t timeoutMs = (uint16_t)currentTimeout * 1000;
  unsigned long idleDurationMs = now - lastActivityTime;
  
  // Check if idle timeout exceeded
  if (idleDurationMs >= timeoutMs) {
    LOG_INFO(LogTag::APP, "[IDLE] Idle timeout reached (%lums >= %ums)", 
             idleDurationMs, timeoutMs);
    return true;  // Signal auto-return
  }
  
  return false;
}

void notifyActivity(unsigned long now) {
  lastActivityTime = now;
}

bool isIdle(unsigned long now) {
  if (currentTimeout == IdleTimeout::OFF) {
    return false;
  }
  
  uint16_t timeoutMs = (uint16_t)currentTimeout * 1000;
  unsigned long idleDurationMs = now - lastActivityTime;
  
  return idleDurationMs >= timeoutMs;
}

bool isIdleReturnCountdown(unsigned long now) {
  if (idleReturnBlocked || currentTimeout == IdleTimeout::OFF) {
    return false;
  }
  
  uint16_t timeoutMs = (uint16_t)currentTimeout * 1000;
  unsigned long idleDurationMs = now - lastActivityTime;
  
  // Return true if we're in the countdown phase (last 3 seconds before timeout)
  return (idleDurationMs >= (timeoutMs - FEEDBACK_LEAD_TIME_MS)) &&
         (idleDurationMs < timeoutMs);
}

uint8_t secondsUntilIdleReturn(unsigned long now) {
  if (idleReturnBlocked || currentTimeout == IdleTimeout::OFF) {
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
