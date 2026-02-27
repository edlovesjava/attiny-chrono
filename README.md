# ATtiny85 Chronograph / Timer

This library represents an ATtiny85 configured as a chronograph / timer

## Components

- ATtiny85 MCU
- Two buttons
- OLED 128x64 SSD12306
- Active piezo buzzer
- RTC DS3231
- Battery (3xAAA)

## Functions

- Clock (24 or 12) mode
- with Alarm (set as absolute time for alarm)
- Countdown timer (same mechansim as alarm using RTC but shows countdown time until alarm, set as time duration)
- Stopwatch / lap time
- Set clock and clock display mode
- Set timer or alarm

## Features

- deep sleep, wake on button press
- low light level of OLED
- Flash or buzz

## MODES

1. Clock 
2. Timer 
3. Stopwatch
4. Set

## Set sub menu

1. Set Clock
   - set hour
   - set minute
   - set am/pm (unless 24)
3. Alarm
   - set hour
   - set minute
   - enable/disable
5. Display
   - clock (24/12)
   - clock on always, clock sleep

## Two button operation
- Menu/set flow
  - Long A - mode menu,
  - Short A (-) or B (+) select,
  - Long A confirm, Long B back/cancel
  - Bottom display shows options, carrat shows selected
- Clock mode



