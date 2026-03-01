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
