# ATtiny85 Chronograph - Hardware Setup

## ATtiny85 Pinout (DIP-8)

```
          ATtiny85
         +---U---+
    PB5  | 1   8 |  VCC (5V)
    PB3  | 2   7 |  PB2 (SCL)
    PB4  | 3   6 |  PB1 (Buzzer)
    GND  | 4   5 |  PB0 (SDA)
         +-------+
```

## Pin Assignments

| Pin | Function | Notes |
|-----|----------|-------|
| PB0 | SDA (I2C data) | Shared bus: OLED + DS3231 |
| PB1 | Buzzer | Active-low (LOW = sound) |
| PB2 | SCL (I2C clock) | Shared bus: OLED + DS3231 |
| PB3 | Button A (SET) | To GND via momentary switch |
| PB4 | Button B (START) + SQW | Shared: button to GND + DS3231 SQW, diode-isolated |
| PB5 | Unused | |

## I2C Bus

Two devices share the I2C bus on PB0 (SDA) / PB2 (SCL):

| Device | Address | Description |
|--------|---------|-------------|
| SSD1306 OLED | 0x3C | 128x64 yellow/blue dual-color display |
| DS3231 RTC | 0x68 | Real-time clock with battery backup |

### Pull-up Resistors

4.7k ohm from SDA to VCC and 4.7k ohm from SCL to VCC. Required -- the ATtiny85's internal pull-ups are too weak for reliable I2C.

## OLED Display

- 128x64 pixel SSD1306, dual-color (top 16px yellow, bottom 48px blue)
- Must initialize with: `oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br)`
- Default `oled.begin()` does NOT work for 128x64

## Buttons

Active-low: each button connects PBn to GND through a momentary switch.

**External pull-up resistors:** 10k ohm from PB3 to VCC and 10k ohm from PB4 to VCC. The code also enables internal pull-ups (`INPUT_PULLUP`), but external 10k resistors give a firmer signal and improve debounce reliability.

### PB3 (Button A only)

```
VCC
 |
10k
 |
PB3 ---+--- Button A --- GND
```

### PB4 (Button B + SQW shared)

PB4 is shared between Button B and the DS3231 SQW (alarm interrupt) output. Diodes prevent the two sources from interfering with each other. Both are active-low -- either a button press or SQW going LOW pulls PB4 LOW and triggers a PCINT wake.

```
       VCC
        |
       10k
        |
PB4 ---+---|>|--- SQW (DS3231)
       |
       +---|>|--- Button B --- GND
```

Diodes: anodes at PB4, cathodes toward SQW and button/GND respectively. Standard signal diodes (1N4148 or similar). When SQW goes LOW (alarm fires), current flows from PB4 through the diode to SQW, pulling PB4 LOW. The button diode is reverse-biased, so no backfeed. Same isolation works in reverse when the button is pressed.

| Threshold | Value |
|-----------|-------|
| Debounce | 50ms |
| Long press | 1000ms |

## Buzzer

- Connected to PB1, active-low
- Initialized HIGH (silent) at startup
- 150ms pulse per beep

## DS3231 RTC

- Battery-backed real-time clock
- Alarm 1 registers used to persist alarm settings across power cycles
- Chained on the same I2C bus as the OLED
- **SQW pin** connected to PB4 via diode -- goes LOW when alarm 1 fires, waking the ATtiny from sleep via PCINT

## Power

- 5V supply
- Auto-sleep after 15s inactivity (power-down mode, ~0.1uA)
- Clock mode uses WDT wake every ~1s to update display while MCU sleeps
- Button press on PB3 or PB4 wakes from any sleep via PCINT

## Programmer

| Setting | Value |
|---------|-------|
| Programmer | Arduino Nano (Arduino as ISP) |
| Port | COM21 (CH340) |
| Protocol | stk500v1 |
| Baud rate | 19200 |

**Important:** The chip must be physically moved between the programmer board and the circuit board. ISP pins (MOSI/MISO/SCK) overlap with I2C pins (SDA/SCL), so programming and I2C cannot coexist.

## Schematic

```
                            +--------+
                       VCC--| 8    1 |--PB5 (NC)
                            |        |
  10k--VCC    BTN_A--GND----| 2(PB3) |
                   PB4 node-| 3(PB4) |
                       GND--| 4    6 |--PB1--Buzzer--GND
                            |   5    |--PB0 (SDA)--+--4.7k--VCC
                            |   7    |--PB2 (SCL)--+--4.7k--VCC
                            +--------+             |
                                                   |
                                            +------+------+
                                            |             |
                                          OLED         DS3231
                                         (0x3C)        (0x68)
                                                        |
                                                       SQW

  PB4 detail (diode-isolated shared pin):

       VCC
        |
       10k
        |
  PB4--+--|>|-- SQW (DS3231, active-low alarm output)
       |
       +--|>|-- BTN_B -- GND

  Diodes: 1N4148 or similar, anodes at PB4
```
