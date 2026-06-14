#pragma once

#include <Arduino.h>
#include "app_state.h"

/**
 * NavigationManager provides intelligent navigation with:
 * - Stack-based screen management
 * - Smart back navigation (short press)
 * - Long press home detection and handling
 * - Visual feedback during long press
 * - Centralized navigation logic
 */
namespace NavigationManager {

/// Initialize navigation manager with HOME screen
void begin();

/// Update navigation state each frame (call in loop)
/// Returns true if long press home is detected
bool update(unsigned long now);

/// Push new screen onto stack (enter new screen)
void enter(AppState state);

/// Pop current screen, return to previous (back button short press)
void back();

/// Immediately return to HOME screen (back button long press)
void goHome();

/// Check if we're at the home screen
bool isAtHome();

/// Get current state
AppState current();

/// Get previous state (for resume-after-event scenarios)
AppState previous();

/// Get current stack depth
uint8_t stackDepth();

/// Check if long press is in progress
bool isLongPressInProgress();

/// Get long press elapsed time (0 if not pressed)
unsigned long longPressElapsedMs();

/// Reset navigation to home only
void reset();

/// Set the home screen (default: STATE_CLOCK)
void setHomeScreen(AppState homeState);

/// Set long press duration threshold (default: 1000ms)
void setLongPressDuration(unsigned long durationMs);

/// Set long press visual feedback threshold (default: 500ms)
void setVisualFeedbackThreshold(unsigned long thresholdMs);

/// Check if visual feedback should be shown
bool shouldShowVisualFeedback();

/// Debug: Print current stack to Serial
void debugPrint();

}  // namespace NavigationManager
