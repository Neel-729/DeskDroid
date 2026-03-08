\# DeskDroid Firmware

\## Version 1.0 – Initial Release

\### Overview

This is the initial release of the DeskDroid firmware, version 1.0. This firmware provides essential productivity tools, including a real-time clock, a countdown timer, a stopwatch, and a motivational quote display. The system is operated using a rotary encoder, while a 16×2 I²C LCD is used for displaying information.

---

\### Key Features

\*\*Real-Time Clock\*\*

\* Utilizes a DS1307 Real-Time Clock module.

\* Displays time, day, and date on the LCD.

\*\*Timer\*\*

\* Countdown timer with adjustable time from 1 to 99 minutes.

\* Audible buzzer notification when the countdown ends.

\*Stopwatch\*\*

\* Allows starting, pausing, and resetting a stopwatch.

\* Displays time elapsed in minutes, seconds, and milliseconds.

\*\*Quote Display\*\*

\* Stores motivational quotes in flash memory.

\* Scrolls quotes on the clock display.

\*\*Rotary Encoder\*\*

\* Utilizes a rotary encoder for user interface.

\* Short press and long press actions are used.

\*\*Audible Buzzer\*\*

\* Audible notification of button presses and countdown completion.

---

\### System Modes

The system has the following user interface modes:

\* Clock

\* Timer

\* Stopwatch

\* Reminders \*(Placeholder)\*

\* Settings \*(Placeholder)\*

---

\### Hardware Supported

The firmware supports the following hardware components:

\* ESP32 DevKit V1

\* DS1307 Real-Time Clock Module

\* 16×2 I²C LCD Display

\* Rotary Encoder

\* Buzzer

---

\### Notes

\* The firmware architecture is \*\*non-blocking, using the millis() function\*\*.

