# DS3231 RTC Integration Plan

## Context

The ATtiny85 chronograph has Timer and Stopwatch modes working. The DS3231 RTC is wired on the shared I2C bus (chained with the OLED). This plan adds Clock mode with time display, a button-based time-setting UI, and an alarm feature -- all via raw I2C register access through a local `DS3231_Tiny` library to keep main.cpp focused on UI/logic.

**Starting point:** 6540 / 8192 bytes flash (79.8%), 209 / 512 bytes RAM (40.8%)
**Target:** < 7800 bytes flash after all tasks
**Files:** `lib/DS3231_Tiny/DS3231_Tiny.h`, `lib/DS3231_Tiny/DS3231_Tiny.cpp`, `src/main.cpp`

### Hardware: DS3231 INT sharing with BTN_B (PB4)

The DS3231 INT/SQW pin is wired to PB4 via a diode-OR circuit, sharing the pin with BTN_B:

```
DS3231 INT ---|\|---+--- PB4 (internal pull-up)
              D1    |
BTN_B -------|\|---+
              D2
```

Both diodes (1N4148 or similar) with cathodes toward PB4. Either source pulling low triggers the existing PCINT4 wake interrupt. On wake, we read the DS3231 status register (0x0F) to distinguish alarm from button press. This enables **alarm wake from sleep in any mode**.

---

### Task 8: Create DS3231_Tiny library and test RTC hardware

**Step 1:** Create `lib/DS3231_Tiny/DS3231_Tiny.h`:

```cpp
#ifndef DS3231_TINY_H
#define DS3231_TINY_H

#include <Arduino.h>

#define DS3231_ADDR 0x68

// Time read/write
void rtcRead(uint8_t &hour, uint8_t &min, uint8_t &sec);
void rtcWrite(uint8_t hour, uint8_t min, uint8_t sec);

// Alarm 1 (matches on hour:min:00 daily)
void rtcSetAlarm(uint8_t hour, uint8_t min);
void rtcDisableAlarm();
bool rtcCheckAlarm();
void rtcClearAlarm();

#endif
```

**Step 2:** Create `lib/DS3231_Tiny/DS3231_Tiny.cpp`:

```cpp
#include "DS3231_Tiny.h"
#include <TinyWireM.h>

static uint8_t bcdToDec(uint8_t val) { return (val / 16 * 10) + (val & 0x0F); }
static uint8_t decToBcd(uint8_t val) { return (val / 10 * 16) + (val % 10); }

void rtcRead(uint8_t &hour, uint8_t &min, uint8_t &sec) {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x00);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 3);
  sec  = bcdToDec(TinyWireM.read() & 0x7F);
  min  = bcdToDec(TinyWireM.read() & 0x7F);
  hour = bcdToDec(TinyWireM.read() & 0x3F);
}

void rtcWrite(uint8_t hour, uint8_t min, uint8_t sec) {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x00);
  TinyWireM.write(decToBcd(sec));
  TinyWireM.write(decToBcd(min));
  TinyWireM.write(decToBcd(hour));
  TinyWireM.endTransmission();
}

void rtcSetAlarm(uint8_t hour, uint8_t min) {
  // Alarm 1 registers 0x07-0x0A
  // Match hours + minutes + seconds=00, ignore day (A1M4=1)
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x07);
  TinyWireM.write(0x00);            // seconds=00, A1M1=0
  TinyWireM.write(decToBcd(min));   // minutes,    A1M2=0
  TinyWireM.write(decToBcd(hour));  // hours,      A1M3=0
  TinyWireM.write(0x80);            // A1M4=1 (don't match day)
  TinyWireM.endTransmission();
  // Enable alarm 1 interrupt: INTCN=1, A1IE=1
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.write(0x05);
  TinyWireM.endTransmission();
  rtcClearAlarm();
}

void rtcDisableAlarm() {
  // INTCN=1, A1IE=0
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.write(0x04);
  TinyWireM.endTransmission();
  rtcClearAlarm();
}

bool rtcCheckAlarm() {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0F);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  return TinyWireM.read() & 0x01;
}

void rtcClearAlarm() {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0F);
  TinyWireM.write(0x00);
  TinyWireM.endTransmission();
}
```

**Step 3:** Add `#include <DS3231_Tiny.h>` to `src/main.cpp` (after existing includes).

Add RTC time cache variables in main.cpp (after stopwatch variables):

```cpp
uint8_t rtcHour, rtcMin, rtcSec;
```

**Step 4:** Add RTC test in `setup()` (after `oled.on()`):

```cpp
  rtcRead(rtcHour, rtcMin, rtcSec);
  oled.clear();
  oled.setCursor(0, 0);
  oled.print("RTC TEST");
  oled.setCursor(0, 3);
  if (rtcHour < 10) oled.print("0");
  oled.print(rtcHour);
  oled.print(":");
  if (rtcMin < 10) oled.print("0");
  oled.print(rtcMin);
  oled.print(":");
  if (rtcSec < 10) oled.print("0");
  oled.print(rtcSec);
  oled.on();
  delay(3000);
```

**Step 5:** `pio run` -- BUILD SUCCESS. PlatformIO auto-discovers `lib/DS3231_Tiny/`.

**Step 6:** `pio run -t upload` -- screen shows "RTC TEST" with time for 3s, then normal chronograph. If time shows `00:00:00` that's OK (RTC never set). If it hangs, check I2C wiring.

**Step 7:** Commit: `feat: add DS3231_Tiny library and verify RTC on shared I2C bus`

---

### Task 9: Add Clock mode with RTC display

**Step 1:** Remove the RTC test block from `setup()` (served its purpose).

**Step 2:** Expand Mode enum:

```cpp
enum Mode { MODE_TIMER, MODE_STOPWATCH, MODE_CLOCK, MODE_COUNT };
```

Mode cycling via `(currentMode + 1) % MODE_COUNT` automatically picks up the new mode.

**Step 3:** Add alarm variables (declared now, used in Task 11):

```cpp
uint8_t alarmHour = 0, alarmMin = 0;
bool alarmEnabled = false;
```

**Step 4:** Add clock display in `updateDisplay()` (after stopwatch block, before `oled.on()`):

```cpp
  if (currentMode == MODE_CLOCK) {
    oled.setCursor(0, 0);
    oled.print("CLOCK");

    if (alarmEnabled) {
      oled.setCursor(64, 0);
      oled.print("A");
      if (alarmHour < 10) oled.print("0");
      oled.print(alarmHour);
      oled.print(":");
      if (alarmMin < 10) oled.print("0");
      oled.print(alarmMin);
    }

    oled.setCursor(0, 3);
    if (rtcHour < 10) oled.print("0");
    oled.print(rtcHour);
    oled.print(":");
    if (rtcMin < 10) oled.print("0");
    oled.print(rtcMin);
    oled.print(":");
    if (rtcSec < 10) oled.print("0");
    oled.print(rtcSec);

    if (alarmEnabled) {
      drawSoftKeys("TIME", "OFF");
    } else {
      drawSoftKeys("TIME", "ALARM");
    }
  }
```

**Step 5:** Add 1Hz RTC polling in `loop()` (after stopwatch refresh, before repeating alarm):

```cpp
  if (currentMode == MODE_CLOCK && subState == SUB_IDLE) {
    static uint32_t lastClockRefresh = 0;
    if (millis() - lastClockRefresh >= 1000) {
      lastClockRefresh = millis();
      rtcRead(rtcHour, rtcMin, rtcSec);
      updateDisplay();
    }
  }
```

**Step 6:** Auto-sleep: clock mode stays awake. Add `&& currentMode != MODE_CLOCK` to sleep guard.

**Step 7:** Read RTC on wake -- add `rtcRead(rtcHour, rtcMin, rtcSec);` in wake handler before `updateDisplay()`.

**Step 8:** `pio run` -- BUILD SUCCESS, flash +200-350 bytes

**Step 9:** `pio run -t upload` -- long-press A cycles TIMER -> STOPWATCH -> CLOCK. Clock shows HH:MM:SS updating each second. Soft-keys show "TIME" / "ALARM". Clock mode doesn't auto-sleep.

**Step 10:** Commit: `feat: add clock mode with RTC display and 1Hz polling`

---

### Task 10: Add time-setting UI (unified for clock + alarm)

The setting handler is built unified from the start -- a `settingAlarm` flag controls whether we write to the RTC or save alarm values. This avoids duplicate code.

**Step 1:** Add setting state variables in main.cpp (after alarm variables):

```cpp
uint8_t settingField;             // 0=hour, 1=min
uint8_t settingHour, settingMin;  // temp values during edit
bool settingAlarm;                // true=alarm, false=clock
```

**Step 2:** Restrict mode cycling to SUB_IDLE only (long-press A is "cancel" during setting):

```cpp
  // Was: if (evtA == EVT_LONG) {
  if (evtA == EVT_LONG && subState == SUB_IDLE) {
```

**Step 3:** Add clock button handling in `loop()` (after stopwatch block):

```cpp
  if (currentMode == MODE_CLOCK) {
    if (subState == SUB_IDLE) {
      if (evtA == EVT_SHORT) {
        // Enter time-setting
        rtcRead(rtcHour, rtcMin, rtcSec);
        settingHour = rtcHour;
        settingMin = rtcMin;
        settingField = 0;
        settingAlarm = false;
        subState = SUB_SETTING;
        lastActivity = millis();
        updateDisplay();
      }
    }
    else if (subState == SUB_SETTING) {
      // Unified setting handler (clock or alarm)
      if (evtA == EVT_SHORT) {
        if (settingField == 0) settingHour = (settingHour + 1) % 24;
        else settingMin = (settingMin + 1) % 60;
        lastActivity = millis();
        updateDisplay();
      }
      if (evtB == EVT_SHORT) {
        if (settingField == 0) settingHour = (settingHour == 0) ? 23 : settingHour - 1;
        else settingMin = (settingMin == 0) ? 59 : settingMin - 1;
        lastActivity = millis();
        updateDisplay();
      }
      if (evtA == EVT_LONG) {
        // Cancel
        subState = SUB_IDLE;
        lastActivity = millis();
        rtcRead(rtcHour, rtcMin, rtcSec);
        updateDisplay();
      }
      if (evtB == EVT_LONG) {
        if (settingField == 0) {
          settingField = 1;
          beep();
        } else {
          // Save
          if (settingAlarm) {
            alarmHour = settingHour;
            alarmMin = settingMin;
            alarmEnabled = true;
            rtcSetAlarm(alarmHour, alarmMin);
          } else {
            rtcWrite(settingHour, settingMin, 0);
          }
          subState = SUB_IDLE;
          beep();
          rtcRead(rtcHour, rtcMin, rtcSec);
        }
        lastActivity = millis();
        updateDisplay();
      }
    }
  }
```

**Step 4:** Add setting display in `updateDisplay()` clock block (before the idle display):

```cpp
  if (currentMode == MODE_CLOCK) {
    if (subState == SUB_SETTING) {
      oled.setCursor(0, 0);
      if (settingAlarm) {
        oled.print(settingField == 0 ? "ALM HR" : "ALM MIN");
      } else {
        oled.print(settingField == 0 ? "SET HOUR" : "SET MIN");
      }
      oled.setCursor(0, 3);
      uint8_t val = (settingField == 0) ? settingHour : settingMin;
      if (val < 10) oled.print("0");
      oled.print(val);
      // Preview full time
      oled.setCursor(64, 3);
      if (settingHour < 10) oled.print("0");
      oled.print(settingHour);
      oled.print(":");
      if (settingMin < 10) oled.print("0");
      oled.print(settingMin);
      drawSoftKeys("+1", "-1   OK>");
    } else {
      // ... existing idle display (CLOCK + HH:MM:SS + soft-keys) ...
    }
  }
```

**Step 5:** Add `subState != SUB_SETTING` to auto-sleep guard.

**Step 6:** `pio run` -- BUILD SUCCESS, flash +300-500 bytes

**Step 7:** `pio run -t upload` -- In clock mode: Short A enters "SET HOUR", A/B adjust (+/-), Long B advances to "SET MIN", Long B saves to RTC (beep). Long A cancels. Preview shows full HH:MM during edit.

**Step 8:** Commit: `feat: add time-setting UI with hour/minute adjustment and RTC write`

---

### Task 11: Add alarm functionality (hardware INT via PB4 diode-OR)

The DS3231 hardware alarm drives the INT pin low, which wakes the MCU through the diode-OR circuit on PB4. This lets the alarm fire **from any mode, even sleep**. All RTC alarm register operations use the `DS3231_Tiny` library functions.

**Step 1:** Add alarm entry/toggle in clock SUB_IDLE handler (after the Short A handler):

```cpp
      if (evtB == EVT_SHORT) {
        if (alarmEnabled) {
          // Disable alarm
          alarmEnabled = false;
          rtcDisableAlarm();
        } else {
          // Enter alarm-setting mode
          settingHour = alarmHour;
          settingMin = alarmMin;
          settingField = 0;
          settingAlarm = true;
          subState = SUB_SETTING;
        }
        lastActivity = millis();
        updateDisplay();
      }
```

Short B when alarm off: enters alarm setting. Short B when alarm on: disables alarm.

**Step 2:** Add alarm check at **top of `loop()`**, before button processing.

This runs every iteration but only does I2C when PB4 is actually low AND alarm is enabled -- zero overhead in the normal case:

```cpp
  // Check for DS3231 hardware alarm (INT pulls PB4 low via diode)
  if (alarmEnabled && !digitalRead(BTN_START)) {
    if (rtcCheckAlarm()) {
      rtcClearAlarm();
      // Reset BTN_B state to prevent phantom press from INT
      btnB = { BTN_START, false, false, 0, false };
      subState = SUB_DONE;
      beep();
      rtcRead(rtcHour, rtcMin, rtcSec);
      updateDisplay();
    }
  }
```

This works in **any mode** (Timer, Stopwatch, Clock) and also handles wake-from-sleep (INT triggers PCINT wake, then this check fires on the first loop iteration).

**Step 3:** Update wake handler to check alarm FIRST (before updateDisplay):

In the `if (wakeFlag)` block, add after I2C/OLED reinit, before `updateDisplay()`:

```cpp
    // Check if alarm caused the wake
    if (alarmEnabled && rtcCheckAlarm()) {
      rtcClearAlarm();
      subState = SUB_DONE;
      beep();
    }
    rtcRead(rtcHour, rtcMin, rtcSec);  // always read time on wake
```

**Step 4:** Add global alarm dismiss (for alarm firing outside clock mode). Add before mode-specific button handling, after the mode-cycling block:

```cpp
  // Global alarm dismiss (for alarm firing outside clock mode)
  if (subState == SUB_DONE && currentMode != MODE_TIMER &&
      (evtA != EVT_NONE || evtB != EVT_NONE)) {
    subState = SUB_IDLE;
    lastActivity = millis();
    updateDisplay();
  }
```

Timer mode already handles its own SUB_DONE dismiss.

And in clock mode, add a SUB_DONE handler:

```cpp
    else if (subState == SUB_DONE) {
      if (evtA != EVT_NONE || evtB != EVT_NONE) {
        subState = SUB_IDLE;
        lastActivity = millis();
        rtcRead(rtcHour, rtcMin, rtcSec);
        updateDisplay();
      }
    }
```

**Step 5:** Add alarm display in `updateDisplay()` clock block (new case before SUB_SETTING):

```cpp
    if (subState == SUB_DONE) {
      oled.setCursor(0, 0);
      oled.print("* ALARM *");
      oled.setCursor(0, 3);
      if (alarmHour < 10) oled.print("0");
      oled.print(alarmHour);
      oled.print(":");
      if (alarmMin < 10) oled.print("0");
      oled.print(alarmMin);
      drawSoftKeys("OK", "OK");
    } else if (subState == SUB_SETTING) {
```

**Step 6:** Extend repeating beep to cover all SUB_DONE scenarios:

```cpp
  // Was: if (currentMode == MODE_TIMER && subState == SUB_DONE) {
  if (subState == SUB_DONE) {
```

**Step 7:** `pio run` -- BUILD SUCCESS, flash +250-400 bytes

**Step 8:** `pio run -t upload` -- Test scenarios:

1. **Clock mode alarm:** Set alarm 1 min from now. See "A07:30" indicator. Wait. "* ALARM *" fires, beep every 2s. Press to dismiss.
2. **Alarm from Timer mode:** Set alarm in clock mode, switch to timer mode. Wait. Beep fires, any press dismisses.
3. **Alarm from sleep:** Set alarm, let device sleep (switch to timer mode, wait 15s). Device wakes on alarm, beeps.
4. **Alarm disable:** In clock mode, Short B disables alarm. INT pin releases. No alarm fires.
5. **Button still works:** With alarm set but not firing, BTN_B still works normally for start/stop/etc.

**Step 9:** Commit: `feat: add alarm with DS3231 hardware INT and wake-from-sleep`

---

### Task 12: Flash check and optimization pass

**Step 1:** `pio run 2>&1 | tail -5` -- check flash < 7800 bytes

**Step 2:** If over budget, apply in order:

| Optimization | Savings |
|---|---|
| Extract `print2(val)` helper for zero-padded print (replaces 10+ occurrences) | ~50-80 bytes |
| Shorten strings: `"STOPWTCH"`->`"SWATCH"`, `"* ALARM *"`->`"ALARM!"` | ~20-30 bytes |
| Remove `swLapVisible`, use `swLapSecs > 0` instead | ~10 bytes |

**Step 3:** Full regression test all three modes

**Step 4:** Commit if changes made: `chore: optimize flash usage for RTC integration`

---

## Button Reference (all modes)

| Mode | State | Short A | Short B | Long A | Long B |
|------|-------|---------|---------|--------|--------|
| Timer | IDLE/SET | +1min | -1min | Mode | Start |
| Timer | RUNNING | -- | -- | Mode | Stop |
| Timer | DONE | Dismiss | Dismiss | -- | -- |
| Stopwatch | IDLE | Reset | Start | Mode | -- |
| Stopwatch | RUNNING | Lap | Stop | Mode | -- |
| **Clock** | **IDLE** | **Set time** | **Set/Off alarm** | **Mode** | -- |
| **Clock** | **SETTING** | **+1** | **-1** | **Cancel** | **Next/Save** |
| **Clock** | **DONE** | **Dismiss** | **Dismiss** | -- | -- |

## Estimated Budget

| Metric | Current | After RTC | Limit |
|--------|---------|-----------|-------|
| Flash | 6540 (79.8%) | ~7500-7800 (91-95%) | 8192 |
| RAM | 209 (40.8%) | ~220 (43%) | 512 |

## Design Notes

**Library isolation.** All DS3231 register access lives in `lib/DS3231_Tiny/` (header + source). PlatformIO auto-discovers it. BCD conversion is `static` (internal to the .cpp). The library depends on TinyWireM but does not call `TinyWireM.begin()` -- that's the caller's responsibility. No flash overhead vs inline code; the compiler links identically.

**Alarm works globally.** The DS3231 hardware alarm drives INT low via the diode-OR circuit on PB4, triggering PCINT4. This wakes the MCU from sleep and fires the alarm regardless of which mode is active. The alarm check runs at the top of `loop()` -- it only performs I2C when PB4 is actually low AND alarm is enabled, so there's zero overhead in normal operation.

**Diode-OR on PB4.** Both the DS3231 INT pin and BTN_B share PB4 through diodes (cathodes toward PB4). When the alarm flag is detected, button state is reset to prevent phantom presses from the INT signal. The alarm flag must be cleared promptly to release the INT line.
