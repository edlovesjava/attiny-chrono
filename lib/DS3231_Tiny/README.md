# DS3231_Tiny

A minimal DS3231 RTC driver for the ATtiny85 (and other space-constrained AVRs).

Built for the [attiny-chrono](../../README.md) project, where every byte of flash on the ATtiny85 counts. Existing DS3231 libraries (RTClib, DS3231, RtcDS3231) pull in `<Wire.h>`, `String`, full date/time classes, temperature reading, square-wave generation, and 32kHz output — none of which fit comfortably alongside an OLED driver, font data, and application code on a 8KB part.

This library does only what the chrono needs: read/write time, set a daily alarm, handle the `INT/SQW` interrupt line. Date and Alarm 2 are behind compile-time flags so they cost nothing when unused.

## Dependencies

- [`adafruit/TinyWireM`](https://github.com/adafruit/TinyWireM) — USI-based I2C master for ATtiny

Uses `TinyWireM` directly rather than `Wire`, since the ATtiny85 has no hardware TWI.

## Usage

```cpp
#include <TinyWireM.h>
#include "DS3231_Tiny.h"

void setup() {
  TinyWireM.begin();

  rtcWrite(14, 30, 0);          // set 14:30:00
  rtcSetAlarm(7, 0);            // alarm daily at 07:00
}

void loop() {
  uint8_t h, m, s;
  rtcRead(h, m, s);

  if (rtcCheckAlarm()) {
    // ... fire alarm ...
    rtcClearAlarm();
  }
}
```

## API

### Time
| Function | Purpose |
|---|---|
| `rtcRead(h, m, s)` | Read current time (24-hour) |
| `rtcWrite(h, m, s)` | Set current time (24-hour) |

### Alarm 1 (matches `HH:MM:00` daily)
| Function | Purpose |
|---|---|
| `rtcSetAlarm(h, m)` | Arm alarm; enables `A1IE`+`INTCN` so the `INT/SQW` pin pulls low on match |
| `rtcReadAlarm(h, m)` | Read alarm time; returns `true` if enabled |
| `rtcDisableAlarm()` | Clear `A1IE` and the pending flag |
| `rtcCheckAlarm()` | `true` if `A1F` is set (alarm fired) |
| `rtcClearAlarm()` | Clear `A1F` (call after handling the alarm or to deassert `INT/SQW`) |

All control-register writes preserve the other alarm's `IE` bit, so Alarm 1 and Alarm 2 can be used independently.

## Optional features

Define the flag **before** including the header to enable the corresponding code:

```cpp
#define DS3231_DATE
#define DS3231_ALARM2
#include "DS3231_Tiny.h"
```

| Flag | Adds |
|---|---|
| `DS3231_DATE` | `rtcReadDate(d, m, y)` / `rtcWriteDate(d, m, y)` — day-of-week is skipped |
| `DS3231_ALARM2` | `rtcSetAlarm2`, `rtcReadAlarm2`, `rtcDisableAlarm2`, `rtcCheckAlarm2`, `rtcClearAlarm2` (matches `HH:MM` — Alarm 2 has minute resolution) |

Because the flags are checked in both the header and the `.cpp`, they must be defined consistently across the whole build — easiest to put them in `platformio.ini` as `build_flags = -DDS3231_DATE`.

## Notes

- All values are plain `uint8_t` in decimal; BCD conversion is internal.
- 12/24-hour mode: writes always use 24-hour format (bit 6 of the hour register stays 0). Reads mask bit 6, so a clock previously set to 12-hour mode by another library will read back wrong until rewritten.
- `rtcSetAlarm` forces `INTCN=1`, so the `SQW` square-wave output is disabled. If you want square-wave output you'll need to manage register `0x0E` yourself.
- No oscillator-stopped (`OSF`) handling — the application is expected to detect a fresh power-up via its own means and call `rtcWrite` to seed the time.
