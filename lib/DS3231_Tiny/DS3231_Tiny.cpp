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
  // Enable alarm 1: INTCN=1, A1IE=1, preserve A2IE
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  uint8_t ctrl = TinyWireM.read();
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.write((ctrl | 0x05) & 0x07);  // set A1IE+INTCN, preserve A2IE
  TinyWireM.endTransmission();
  rtcClearAlarm();
}

bool rtcReadAlarm(uint8_t &hour, uint8_t &min) {
  // Read alarm 1 min (0x08) and hour (0x09)
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x08);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 2);
  min  = bcdToDec(TinyWireM.read() & 0x7F);
  hour = bcdToDec(TinyWireM.read() & 0x3F);
  // Check A1IE in control register (0x0E bit 0)
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  return TinyWireM.read() & 0x01;
}

void rtcDisableAlarm() {
  // Clear A1IE, preserve rest
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  uint8_t ctrl = TinyWireM.read();
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.write(ctrl & ~0x01);  // clear A1IE, preserve A2IE+INTCN
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
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  uint8_t status = TinyWireM.read();
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0F);
  TinyWireM.write(status & ~0x01);  // clear A1F, preserve A2F
  TinyWireM.endTransmission();
}

#ifdef DS3231_DATE
void rtcReadDate(uint8_t &day, uint8_t &month, uint8_t &year) {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x04);  // skip dow (0x03)
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 3);
  day   = bcdToDec(TinyWireM.read() & 0x3F);
  month = bcdToDec(TinyWireM.read() & 0x1F);  // mask century bit
  year  = bcdToDec(TinyWireM.read());
}

void rtcWriteDate(uint8_t day, uint8_t month, uint8_t year) {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x04);
  TinyWireM.write(decToBcd(day));
  TinyWireM.write(decToBcd(month));
  TinyWireM.write(decToBcd(year));
  TinyWireM.endTransmission();
}
#endif

#ifdef DS3231_ALARM2
void rtcSetAlarm2(uint8_t hour, uint8_t min) {
  // Alarm 2 registers 0x0B-0x0D
  // Match hours + minutes, ignore day (A2M4=1)
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0B);
  TinyWireM.write(decToBcd(min));     // A2M2=0
  TinyWireM.write(decToBcd(hour));    // A2M3=0
  TinyWireM.write(0x80);              // A2M4=1 (don't match day)
  TinyWireM.endTransmission();
  // Enable alarm 2: INTCN=1, A2IE=1, preserve A1IE
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  uint8_t ctrl = TinyWireM.read();
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.write((ctrl | 0x06) & 0x07);  // set A2IE+INTCN, preserve A1IE
  TinyWireM.endTransmission();
  rtcClearAlarm2();
}

bool rtcReadAlarm2(uint8_t &hour, uint8_t &min) {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0B);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 2);
  min  = bcdToDec(TinyWireM.read() & 0x7F);
  hour = bcdToDec(TinyWireM.read() & 0x3F);
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  return TinyWireM.read() & 0x02;  // A2IE is bit 1
}

void rtcDisableAlarm2() {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  uint8_t ctrl = TinyWireM.read();
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0E);
  TinyWireM.write(ctrl & ~0x02);  // clear A2IE, preserve rest
  TinyWireM.endTransmission();
  rtcClearAlarm2();
}

bool rtcCheckAlarm2() {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0F);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  return TinyWireM.read() & 0x02;  // A2F is bit 1
}

void rtcClearAlarm2() {
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0F);
  TinyWireM.endTransmission();
  TinyWireM.requestFrom(DS3231_ADDR, 1);
  uint8_t status = TinyWireM.read();
  TinyWireM.beginTransmission(DS3231_ADDR);
  TinyWireM.write(0x0F);
  TinyWireM.write(status & ~0x02);  // clear A2F, preserve A1F
  TinyWireM.endTransmission();
}
#endif
