# Custom Font Optimization Plan

## Context

The ATtiny85 chronograph has Timer, Stopwatch, and Clock modes. The DS3231 RTC is on the shared I2C bus (chained with OLED). RTC library is in `lib/DS3231_Tiny/`. Clock mode includes time display with 1Hz polling, time-setting UI, and WDT-based low-power sleep.

**Current state:** 8016 / 8192 bytes flash (97.9%), 291 / 512 bytes RAM (56.8%)
**Problem:** Only 176 bytes free. Alarm feature (Task 11) needs ~250-400 bytes.
**Solution:** Custom font to reclaim ~576 bytes before adding alarm.

## Progress

| Task | Status | Notes |
|------|--------|-------|
| Task 8: RTC hardware test | DONE | Library created, I2C verified |
| Task 9: Clock mode + display | DONE | 1Hz polling, mode cycling works |
| Task 10: Time-setting UI + low-power | CODED | Not yet uploaded/tested on hardware |
| Task 10.5: Custom font optimization | **NEXT** | Reclaim ~576 bytes for alarm |
| Task 11: Alarm feature | PENDING | Needs flash from Task 10.5 |
| Task 12: Final optimization | PENDING | print2() already applied |

**Files:**
- `src/main.cpp` -- all application code
- `lib/DS3231_Tiny/DS3231_Tiny.h` / `.cpp` -- RTC library
- `src/font_chrono.h` -- custom font (to be created in Task 10.5)

---

## Font Analysis

### How FONT8X16 works in Tiny4kOLED

- Defined in `.pio/libdeps/attiny85/Tiny4kOLED/src/font8x16.h`
- **95 glyphs** (ASCII 32-126) × 16 bytes each = **1,520 bytes** in PROGMEM
- Indexed by `(ascii_code - first) * width * height` -- requires **contiguous** ASCII range
- `DCfont` struct: `{bitmap_ptr, width=8, height=2, first=32, last=126, 0, 0, 0}`
- With `-fdata-sections` + `--gc-sections`, only the font actually referenced via `setFont()` gets linked. Other fonts in the library are stripped.

### Characters actually used in OLED output (39 total)

| Category | Characters |
|----------|-----------|
| Digits | 0 1 2 3 4 5 6 7 8 9 |
| Letters | A B C D E G H I K L M N O P R S T U V W Y Z |
| Symbols | `:` `+` `-` `>` `*` (space) |

### Custom font strategy

A contiguous font covering **ASCII 32-90** (space through Z) = **59 glyphs × 16 = 944 bytes**.

- All 39 used characters fall within this range
- Unused slots (!, #, $, etc.) contain zero data (display as blank -- harmless)
- **Savings: 1,520 - 944 = 576 bytes**

---

## Task 10.5: Custom Font Optimization

**Step 1:** Create `src/font_chrono.h` containing:
- Glyph data for ASCII 32-90 extracted from FONT8X16's `ssd1306xled_font8x16[]` array
- This is literally the **first 59 entries** (first 944 bytes) of the existing array
- DCfont struct with `first=32, last=90`

```cpp
#pragma once
#include <avr/pgmspace.h>
#include <Tiny4kOLED_common.h>

// Custom 8x16 font: ASCII 32-90 (space through Z)
// 59 glyphs x 16 bytes = 944 bytes (vs 1520 for full FONT8X16)
const uint8_t chrono_font_data[] PROGMEM = {
  // Copy first 59 glyph entries (944 bytes) from font8x16.h
  // Lines 26-84 of font8x16.h (ASCII 32 space through ASCII 90 Z)
};

const DCfont chronoFont = {
  (uint8_t *)chrono_font_data,
  8, 2, 32, 90, 0, 0, 0
};

#define FONT_CHRONO (&chronoFont)
```

**Step 2:** In `src/main.cpp`:
- Add `#include "font_chrono.h"` near the top
- Replace all `oled.setFont(FONT8X16)` with `oled.setFont(FONT_CHRONO)` (3 occurrences: setup, wake-from-sleep, wake-from-clock-low-power)

**Step 3:** `pio run` -- verify flash drops by ~576 bytes (from 8016 to ~7440)

**Step 4:** `pio run -t upload` -- test all three modes. Display should look identical.

**Step 5:** Commit: `chore: custom font with reduced ASCII range saves ~576 bytes`

---

## Estimated Budget

| Stage | Flash | Free | RAM |
|-------|-------|------|-----|
| After Task 10 (current) | 8016 (97.9%) | 176 bytes | 291 (56.8%) |
| After Task 10.5 (custom font) | ~7440 (90.8%) | ~752 bytes | 291 (56.8%) |
| After Task 11 (alarm) | ~7700-7800 (94-95%) | ~400-500 bytes | ~300 (58.6%) |

## Execution Order

1. **Upload & test Task 10** (time-setting + clock low-power) -- already coded, needs hardware test
2. **Task 10.5** -- create custom font, verify build drops ~576 bytes, upload & test
3. **Task 11** -- add alarm feature (now fits in budget)
4. **Task 12** -- final flash check and cleanup
