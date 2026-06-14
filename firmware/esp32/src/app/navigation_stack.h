#pragma once

#include <Arduino.h>
#include "app_state.h"

/**
 * NavigationStack maintains a LIFO stack of application states.
 * Supports push/pop operations for back navigation.
 */
namespace NavigationStack {

/// Initialize the navigation stack with HOME screen
void begin();

/// Push a new state onto the stack (enter screen)
/// Returns false if stack is full
bool push(AppState state);

/// Pop the top state from stack (go back)
/// If only HOME remains, returns false (no-op)
bool pop();

/// Peek at the top state without removing
AppState peek();

/// Peek at state at given depth (0 = top of stack, 1 = previous, etc.)
/// Returns HOME if depth is out of bounds
AppState peekAt(uint8_t depth);

/// Get the previous state on the stack
AppState previous();

/// Check if we're at the home screen
bool isAtHome();

/// Get current stack depth (1 = home only)
uint8_t depth();

/// Get maximum stack depth
uint8_t maxDepth();

/// Reset stack to home only
void reset();

/// Debug: Print stack contents to Serial
void debugPrint();

}  // namespace NavigationStack
