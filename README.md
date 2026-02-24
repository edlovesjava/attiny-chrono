# ATtiny85 Chronograph / Timer

This library represents an ATtiny85 configured as a chronograph / timer

## Components

- ATtiny85 MCU
- Three buttons
  - (+) plus button
  - (-) minus button
  - mode button
- OLED 128x64 SSD12306
- Active piezo buzzer
- RTC DS3231

## Functions

- Clock (24 or 12) mode
- with Alarm (set as absolute time for alarm)
- Countdown timer (same mechansim as alarm using RTC but shows countdown time until alarm, set as time duration)
- Stopwatch / lap time
- Set clock and clock display mode

