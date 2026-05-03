# Custom Font for Tiny4kOLED on ATtiny85

How (and why) we shrank the OLED font from 1520 bytes to 432 bytes — and reused unused ASCII slots as icon glyphs to avoid shipping a separate icon array.

This pattern applies to any small AVR project using [Tiny4kOLED](https://github.com/datacute/Tiny4kOLED) (or any library that draws fonts from a flat `PROGMEM` table indexed by ASCII code).

## The problem

`FONT8X16` from Tiny4kOLED is **1520 bytes** of PROGMEM — 95 glyphs (ASCII 32–126) × 16 bytes each. On an ATtiny85 with 8 KB of flash, that's nearly 19 % of total program space spent on letters most apps never draw.

A common assumption is that the linker will garbage-collect unused glyphs. **It will not.** A font is one contiguous `PROGMEM` array; either the whole array is referenced (via `setFont()`) or the whole array is dropped. Per-glyph DCE doesn't apply.

Tiny4kOLED *does* let `--gc-sections` strip *unreferenced* fonts (so unused FONT16X32 etc. cost nothing), but the one font you actually call `setFont()` on lands whole.

## The trick

`DCfont` indexes glyphs by `(ascii - first) * width * pages`. So the font's flash cost is:

```
(last - first + 1) * width * pages   bytes
```

To shrink the font you do two things:

1. **Pick the smallest contiguous ASCII range** that covers everything you draw. The range is contiguous — you can't skip codes in the middle.
2. **Repurpose unused codes inside that range** as icon glyphs, so you don't need a separate icon-blitting code path or a second PROGMEM array.

For the chrono we picked **ASCII 32 (space) through 58 (':')** — 27 slots covering space, digits, colon, and 13 punctuation characters we never draw as text. Those 13 punctuation slots became our icon set (hourglass, stopwatch, bell, play, stop, etc.).

```
27 glyphs × 8 px wide × 2 pages = 432 bytes
```

A **1088-byte saving** (~13 % of the ATtiny85's flash) and zero extra code to render the icons — `oled.print('!')` draws an hourglass.

## The format

Tiny4kOLED's 8×16 glyphs are SSD1306 column-major:

- 16 bytes per glyph
- First 8 bytes = upper page (rows 0–7), one byte per column
- Next 8 bytes = lower page (rows 8–15), one byte per column
- Within each byte, **bit 0 = top pixel**, bit 7 = bottom pixel of that page

Hand-rolling these bytes is tedious and error-prone, which is why we use a generator.

## Workflow

The font is generated from pixel art in [`tools/gen_icons.py`](../tools/gen_icons.py):

```python
glyphs[(36, '$ 36 bell (ALARM)')] = """
........
....XX..
....XXx.
....XX.x
....XX.x
....XX.x
....XX..
..XXXX..
.X..XX..
.X..XX..
..XXX...
........
........
........
........
........
"""
```

8 columns × 16 rows, `X` = pixel on, `.` = pixel off. Run the script to regenerate the header:

```bash
python tools/gen_icons.py
# → wrote src/font_chrono.h (27 glyphs, 432 bytes PROGMEM)
```

Commit both `gen_icons.py` and the generated `src/font_chrono.h` so a clean checkout builds without needing Python.

## Digit override

Digits `0`–`9` and `:` are copied byte-for-byte from the canonical `FONT8X16` (see `digit_overrides` in `gen_icons.py`) rather than re-drawn from pixel art. Reasons:

- They're already pixel-perfect at 8×16; redrawing risks regressions in the most-displayed glyphs.
- Lets us cleanly fall back to the stock font during development without visual jitter.

The pixel-art entries for 48–58 in `gen_icons.py` are kept as a *reference / sketch* but are not the source of truth.

## Adapting this to your project

1. **List every character your app prints.** Including settings screens, error states, debug output.
2. **Find the tightest contiguous ASCII range** that covers them. If your set spans say `'A'..'Z'` (65–90) plus digits (48–57) plus space (32), your range is 32–90 — 59 glyphs, 944 bytes. Often you can drop letters by switching labels to icons.
3. **Inventory the unused slots in that range** — those are free icon real estate. ASCII 33–47 (punctuation) and 58–64 typically yield 15–22 unused slots each.
4. **Copy the pattern from `tools/gen_icons.py`**: keep digit/letter glyphs as overrides from the original font (they're already optimised), draw your icons as pixel art.
5. **Update `DCfont`'s `first` and `last`** to match your range. Don't forget — getting these wrong shifts every glyph by an ASCII offset.

### Approximate savings table

| Range | Glyphs | Bytes | Saved vs FONT8X16 |
|---|---|---|---|
| 32–126 (full) | 95 | 1520 | 0 |
| 32–90 (`Z`) | 59 | 944 | 576 |
| 32–58 (`:`) | 27 | 432 | 1088 |
| 48–58 (`0`–`:`) | 11 | 176 | 1344 |

The last row is digit-only — fine for a clock that never displays text.

## Caveats

- **Contiguous range only.** You cannot skip ASCII codes; pay for every slot between `first` and `last`.
- **One font at a time.** `setFont()` is global. Switching fonts mid-frame works but each font you reference costs its full size.
- **Linker GC must be on.** Confirm `-fdata-sections -ffunction-sections` (compile) and `-Wl,--gc-sections` (link) — PlatformIO's `arduino` framework enables these by default. Without them, *all* fonts in Tiny4kOLED link in regardless.
- **Icons in punctuation slots are still ASCII.** `oled.print("Hello!")` will render `Hello🕰` (or whatever sits at code 33). Don't print free-form text through this font.
