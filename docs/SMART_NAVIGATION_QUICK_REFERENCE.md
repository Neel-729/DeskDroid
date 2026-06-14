# Smart Navigation - Quick Reference

## Basic Usage

### Navigate to a New Screen
```cpp
AppNavigation::enter(STATE_SETTINGS_HOME);  // Push onto stack
```

### Go Back to Previous Screen
```cpp
AppNavigation::back();  // Pop from stack
```

### Return to Home Immediately
```cpp
AppNavigation::goHome();  // Reset stack to HOME only
```

### Check Current State
```cpp
AppState current = AppNavigation::current();
bool atHome = AppNavigation::isAtHome();
uint8_t depth = AppNavigation::stackDepth();
```

## Navigation Stack Behavior

```
Action                  Stack Before        Stack After
─────────────────────────────────────────────────────────
enter(SETTINGS)         [HOME]              [HOME, SETTINGS]
enter(RGB)              [HOME, SETTINGS]    [HOME, SETTINGS, RGB]
back()                  [HOME, SETTINGS, RGB] [HOME, SETTINGS]
goHome()                [HOME, SETTINGS, RGB] [HOME]
back() @ HOME           [HOME]              [HOME] (no-op)
```

## Long Press Home Behavior

**Timing:**
- Press: Button pressed
- +500ms: Visual feedback shown (logged to console)
- +1000ms: goHome() triggered, immediate navigation to HOME
- Release: Button released, state reset

**Audio Feedback:**
- Distinct 200ms beep when long press triggers

**Console Logs:**
```
[NAV] Button pressed, monitoring for long press home
[NAV] Visual feedback threshold reached (543ms)
[NAV] Long press home triggered (1023ms)
[NAV] Long press home completed
```

## Configuration

Located in `src/app/application.cpp`:

```cpp
constexpr unsigned long LONG_PRESS_HOME_DURATION = 1000;      // ms
constexpr unsigned long VISUAL_FEEDBACK_THRESHOLD = 500;      // ms
constexpr uint8_t ENCODER_BUTTON_PIN = 5;                     // GPIO pin
```

## Edge Cases

| Scenario | Behavior |
|----------|----------|
| Already at HOME, short back | No-op |
| Already at HOME, long press back | No-op |
| Deeply nested menu, long press back | Immediate jump to HOME |
| Button held < 500ms | No feedback, normal click processing |
| Button held 500-1000ms | Feedback shown, no navigation yet |
| Button held > 1000ms | Navigation triggered, immediate HOME |
| Rapid navigation + long press | Reset to HOME |

## Testing

### Test: Normal Back Navigation
1. Navigate: Home → Settings → RGB
2. Press back (short)
3. Verify: In Settings
4. Press back (short)
5. Verify: In Home

### Test: Long Press Home
1. Navigate: Home → Settings → RGB → Color
2. Hold back button for 1+ second
3. Verify: Immediately at Home (no intermediate screens)
4. Verify: Beep sound heard
5. Verify: Console shows "Long press home triggered"

### Test: Button Release Before Threshold
1. Navigate: Home → Settings
2. Press back button, hold for 600ms, then release
3. Verify: Normal back/click behavior occurs
4. Verify: No "Long press home" in logs

## Architecture Layers

```
┌──────────────────────────────────┐
│  Application Logic               │  (application.cpp)
│  handleEvent(), runTasks()       │
└──────────────────────────────────┘
              ↓
┌──────────────────────────────────┐
│  AppNavigation (Wrapper)         │  (navigation.cpp)
│  enter(), back(), goHome()       │
└──────────────────────────────────┘
              ↓
┌──────────────────────────────────┐
│  NavigationStack (Storage)       │  (navigation_stack.cpp)
│  push(), pop(), peek()           │
└──────────────────────────────────┘
              ↓
┌──────────────────────────────────┐
│  Task System Integration         │  (scheduler)
│  runNavMonitorTask()             │
└──────────────────────────────────┘
              ↓
┌──────────────────────────────────┐
│  Hardware (GPIO)                 │  (encoder_driver)
│  Digital pin 5 (button)          │
└──────────────────────────────────┘
```

## Future Enhancements

Possible improvements:
1. Visual "Returning Home..." indicator on screen after 500ms
2. Dialog-aware navigation (save/discard prompts)
3. Per-screen long-press customization
4. Animation during long-press home transition
5. Haptic feedback (vibration) during long press
6. Configurable thresholds via settings
