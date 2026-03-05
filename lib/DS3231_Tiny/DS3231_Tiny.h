#ifndef DS3231_TINY_H
#define DS3231_TINY_H

// Feature flags -- define before including to enable
// #define DS3231_DATE     // date read/write
// #define DS3231_ALARM2   // alarm 2 support

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

#ifdef DS3231_DATE
void rtcReadDate(uint8_t &day, uint8_t &month, uint8_t &year);
void rtcWriteDate(uint8_t day, uint8_t month, uint8_t year);
#endif

#ifdef DS3231_ALARM2
void rtcSetAlarm2(uint8_t hour, uint8_t min);
bool rtcReadAlarm2(uint8_t &hour, uint8_t &min);
void rtcDisableAlarm2();
bool rtcCheckAlarm2();
void rtcClearAlarm2();
#endif

#endif
