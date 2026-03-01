#ifndef DS3231_TINY_H
#define DS3231_TINY_H

#include <Arduino.h>

#define DS3231_ADDR 0x68

// Time read/write
void rtcRead(uint8_t &hour, uint8_t &min, uint8_t &sec);
void rtcWrite(uint8_t hour, uint8_t min, uint8_t sec);

// Alarm 1 (matches on hour:min:00 daily)
void rtcSetAlarm(uint8_t hour, uint8_t min);
bool rtcReadAlarm(uint8_t &hour, uint8_t &min);
void rtcDisableAlarm();
bool rtcCheckAlarm();
void rtcClearAlarm();

#endif
