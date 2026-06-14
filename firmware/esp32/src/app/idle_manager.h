#pragma once

#include <Arduino.h>

/**
 * IdleManager tracks user inactivity and triggers auto-return to dashboard.
 * 
 * Features:
 * - Configurable idle timeout (OFF, 15s, 30s, 60s, 120s, 300s)
 * - Activity detection from encoder/buttons/navigation
 * - Centralized idle tracking
 * - Screens can opt-out of auto-return
 * - Visual feedback before return
 */
namespace IdleManager {

// Idle timeout options (in seconds)
enum class IdleTimeout : uint16_t {
  OFF = 0,
  FIFTEEN_SECONDS = 15,
  THIRTY_SECONDS = 30,
  SIXTY_SECONDS = 60,
  TWO_MINUTES = 120,
  FIVE_MINUTES = 300
};

/// Initialize idle manager
void begin();

/// Update idle state (call every frame)
/// Returns true if auto-return should happen
bool update(unsigned long now);

/// Notify that user activity occurred (resets idle timer)
void notifyActivity(unsigned long now);

/// Check if currently idle (beyond threshold)
bool isIdle(unsigned long now);

/// Check if idle return is in progress (countdown phase)
bool isIdleReturnCountdown(unsigned long now);

/// Get seconds remaining until idle return (0 if not applicable)
uint8_t secondsUntilIdleReturn(unsigned long now);

/// Set the idle timeout
void setIdleTimeout(IdleTimeout timeout);

/// Get the current idle timeout
IdleTimeout getIdleTimeout();

/// Get idle timeout as seconds (0 if disabled)
uint16_t getIdleTimeoutSeconds();

/// Get last activity timestamp
unsigned long getLastActivityTime();

/// Get idle duration in milliseconds
unsigned long getIdleDurationMs(unsigned long now);

/// Temporarily block idle return (e.g., during critical operations)
void blockIdleReturn(bool blocked);

/// Check if idle return is currently blocked
bool isIdleReturnBlocked();

/// Debug: Print current state to Serial
void debugPrint(unsigned long now);

}  // namespace IdleManager
