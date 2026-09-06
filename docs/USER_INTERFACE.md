# User interface behavior

The ESP32 renders two 16-character LCD rows. The rotary encoder is the live navigation/control input. Button debounce is 25 ms; a hold of at least 1000 ms emits `EVENT_LONG_PRESS`; a second click is arbitrated for 180 ms after the first release.

## Main screens

Rotation cycles through `Clock → Timer → Stopwatch → Reminders → Settings` and wraps around. Clock click selects the next quote. The clock display shows time/date and a scrolling quote when quotes are enabled.

## Timer

The timer stores hours, minutes, and seconds. Hours are clamped to 0–99; minutes and seconds to 0–59. A zero duration is normalized to one second by the state store.

| Screen/state | Single click | Double click | Long press |
| --- | --- | --- | --- |
| Timer view, idle | Start | Enter edit | Return home |
| Timer view, running | Pause | Pause and reset | Return home |
| Timer view, paused | Resume | Reset | Return home |
| Timer view, complete | Enter edit | Reset and go Clock | Return home |
| Timer edit | Advance H→M→S | Exit to timer view | Return home |
| Timer alarm | Reset and return to timer view | No screen-specific handling | Return home |

The application’s current event ordering handles a long press through the generic home action before the later screen-specific long-press switch. Therefore a long press normally returns to Clock and does not perform the lower branch’s intended reset/save/toggle behavior. The timer’s underlying state is not changed by the generic navigation action.

## Stopwatch

Single click starts, pauses, or resumes according to state. Double click resets while running or paused; double click while idle does nothing. Long press returns to Clock through the generic handler; the later reset-and-home branch is not reached.

## Reminders

There are five stored reminder slots. On the reminders home screen, click opens the list; on the list, click opens the selected reminder. Rotation selects a slot in the list and edits the selected hour/minute in edit mode. Click advances the edit field. Double click saves and returns from edit, or returns from the list/home levels. Long press returns to Clock through the generic handler; the later toggle-and-save branch is not reached.

The reminder checker runs once per second, triggers at a matching active hour/minute, and prevents duplicate triggering in the same minute. An active reminder alarm beeps in a repeating pattern for up to 60 seconds; any click/double-click/long-press stops it and resumes the previous screen.

## Settings

Settings has 14 menu entries: Backlight, LED Mode, LED Brightness, Relays, Auto Lights, Lights On, Lights Off, Auto Return Home, Buzzer, Quotes, Time Format, Adjust Time, Adjust Date, and About.

Rotation selects entries or changes a value. Click enters a menu/edit level or advances a field. Double click commits/exits the current level. Long press returns to Clock through the generic handler; the later settings save/commit branches are not reached. Schedule/time/date fields blink while editing.

## Important input limitation

The TTP229-BSF scanner is polled every 10 ms, but its key transitions are only printed to Serial. They do not currently drive settings, macros, or other application actions.
