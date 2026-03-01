#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <TinyWireM.h>
#include <Tiny4kOLED.h>
#include <DS3231_Tiny.h>
#include "font_chrono.h"

#define BTN_SET   PB3
#define BTN_START PB4
#define BUZZER    PB1

#define DEBOUNCE_MS    50
#define LONG_PRESS_MS  1000

struct Button {
  uint8_t pin;
  bool lastRaw;
  bool pressed;
  uint32_t pressStart;
  bool handled;
};

enum ButtonEvent { EVT_NONE, EVT_SHORT, EVT_LONG };

Button btnA = { BTN_SET,   false, false, 0, false };
Button btnB = { BTN_START, false, false, 0, false };

ButtonEvent readButton(Button &b) {
  bool raw = !digitalRead(b.pin);
  ButtonEvent evt = EVT_NONE;

  // Detect press start with debounce
  if (raw && !b.pressed) {
    if (!b.lastRaw) {
      b.pressStart = millis();
    } else if (millis() - b.pressStart >= DEBOUNCE_MS) {
      b.pressed = true;
      b.handled = false;
    }
  }

  // Detect long press while held
  if (b.pressed && !b.handled && raw &&
      millis() - b.pressStart >= LONG_PRESS_MS) {
    evt = EVT_LONG;
    b.handled = true;
  }

  // Detect short press on release
  if (b.pressed && !raw) {
    if (!b.handled && millis() - b.pressStart >= DEBOUNCE_MS) {
      evt = EVT_SHORT;
    }
    b.pressed = false;
  }

  b.lastRaw = raw;
  return evt;
}

volatile bool wakeFlag = false;

enum Mode { MODE_TIMER, MODE_STOPWATCH, MODE_CLOCK, MODE_COUNT };
enum SubState { SUB_IDLE, SUB_SETTING, SUB_RUNNING, SUB_DONE };

Mode currentMode = MODE_TIMER;
SubState subState = SUB_IDLE;
bool isSleeping = false;
bool clockLowPower = false;

uint16_t targetSeconds = 0;
uint16_t currentSeconds = 0;
uint32_t lastActivity = 0;

uint32_t swStart = 0;       // millis() when stopwatch started
uint32_t swAccum = 0;       // accumulated ms from previous runs
uint16_t swLapSecs = 0;     // last lap snapshot in seconds
bool swLapVisible = false;   // whether to show lap time

uint8_t rtcHour, rtcMin, rtcSec;

uint8_t alarmHour = 0, alarmMin = 0;
bool alarmEnabled = false;

uint8_t settingField;             // 0=hour, 1=min
uint8_t settingHour, settingMin;  // temp values during edit
bool settingAlarm;                // true=alarm, false=clock
bool alarmFired = false;          // prevent re-trigger within same minute

void goToSleep() {
  oled.off();
  oled.clear();
  oled.on();

  GIMSK |= _BV(PCIE);
  PCMSK |= _BV(PCINT3) | _BV(PCINT4);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();

  sleep_disable();
}

ISR(PCINT0_vect) {
  wakeFlag = true;
}

ISR(WDT_vect) {
  // Just wakes CPU. WDIE auto-clears.
}

void clockSleep() {
  // Enable PCINT for button wake
  GIMSK |= _BV(PCIE);
  PCMSK |= _BV(PCINT3) | _BV(PCINT4);
  // Enable WDT interrupt, ~1s
  cli();
  WDTCR |= _BV(WDCE) | _BV(WDE);
  WDTCR = _BV(WDIE) | _BV(WDP2) | _BV(WDP1);
  sei();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
}

void setup() {
  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH);  // buzzer off (active-low)

  TinyWireM.begin();
  oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
  oled.setFont(FONT_CHRONO);
  oled.clear();
  oled.on();
  alarmEnabled = rtcReadAlarm(alarmHour, alarmMin);
  rtcClearAlarm();  // ensure SQW is HIGH on boot
}

void beep() {
  digitalWrite(BUZZER, LOW);
  delay(150);
  digitalWrite(BUZZER, HIGH);
}

void print2(uint8_t val) {
  if (val < 10) oled.print("0");
  oled.print(val);
}

void drawSoftKeys(const char* left, const char* right) {
  oled.setCursor(0, 6);
  oled.print(left);
  // Right-align the right label (128px width, 8px per char)
  uint8_t rightLen = 0;
  while (right[rightLen]) rightLen++;
  uint8_t rightX = 128 - (rightLen * 8);
  oled.setCursor(rightX, 6);
  oled.print(right);
}

void updateDisplay() {
  oled.clear();
  oled.setCursor(0, 0);

  // Mode name
  if (currentMode == MODE_TIMER) {
    oled.print("TIMER");
  } else if (currentMode == MODE_STOPWATCH) {
    oled.print("STOPWTCH");
  }

  // Current time in upper right (timer/stopwatch only)
  if (currentMode == MODE_TIMER || currentMode == MODE_STOPWATCH) {
    oled.setCursor(88, 0);
    print2(rtcHour);
    oled.print(":");
    print2(rtcMin);
  }

  if (currentMode == MODE_TIMER) {
    oled.setCursor(0, 3);
    uint16_t t = (subState == SUB_RUNNING) ? currentSeconds : targetSeconds;
    print2(t / 60);
    oled.print(":");
    print2(t % 60);

    // Soft-key labels for timer mode
    if (subState == SUB_DONE) {
      drawSoftKeys("OK", "OK");
    } else if (subState == SUB_RUNNING) {
      drawSoftKeys("", "STOP>");
    } else {
      // IDLE or SETTING
      if (targetSeconds == 0) {
        drawSoftKeys("+1m", "");
      } else {
        drawSoftKeys("+1m", "-1m  GO>");
      }
    }
  }

  if (currentMode == MODE_STOPWATCH) {
    uint32_t totalMs = swAccum;
    if (subState == SUB_RUNNING) {
      totalMs += millis() - swStart;
    }
    uint16_t t = (uint16_t)(totalMs / 1000);

    oled.setCursor(0, 3);
    print2(t / 60);
    oled.print(":");
    print2(t % 60);

    if (swLapVisible) {
      oled.setCursor(0, 5);
      oled.print("LAP ");
      print2(swLapSecs / 60);
      oled.print(":");
      print2(swLapSecs % 60);
    }

    // Soft-key labels for stopwatch mode
    if (subState == SUB_RUNNING) {
      drawSoftKeys("LAP", "STOP");
    } else {
      // IDLE
      if (swAccum == 0) {
        drawSoftKeys("", "START");
      } else {
        drawSoftKeys("RESET", "START");
      }
    }
  }

  if (currentMode == MODE_CLOCK) {
    if (subState == SUB_DONE) {
      oled.setCursor(0, 0);
      oled.print("* ALARM *");
      oled.setCursor(0, 3);
      print2(alarmHour);
      oled.print(":");
      print2(alarmMin);
      drawSoftKeys("OK", "OK");
    } else if (subState == SUB_SETTING) {
      oled.setCursor(0, 0);
      if (settingAlarm) {
        oled.print(settingField == 0 ? "ALM HR" : "ALM MIN");
      } else {
        oled.print(settingField == 0 ? "SET HOUR" : "SET MIN");
      }
      oled.setCursor(0, 3);
      print2((settingField == 0) ? settingHour : settingMin);
      oled.setCursor(64, 3);
      print2(settingHour);
      oled.print(":");
      print2(settingMin);
      drawSoftKeys("+1", "-1   OK>");
    } else {
      oled.setCursor(0, 0);
      oled.print("CLOCK");

      if (alarmEnabled) {
        oled.setCursor(64, 0);
        oled.print("A");
        print2(alarmHour);
        oled.print(":");
        print2(alarmMin);
      }

      oled.setCursor(0, 3);
      print2(rtcHour);
      oled.print(":");
      print2(rtcMin);
      oled.print(":");
      print2(rtcSec);

      if (alarmEnabled) {
        drawSoftKeys("TIME", "OFF");
      } else {
        drawSoftKeys("TIME", "ALARM");
      }
    }
  }

  oled.on();
}

void loop() {
  if (!wakeFlag && isSleeping) {
    goToSleep();
  }

  // Clock low-power mode: MCU sleeps, wakes every ~1s to update time
  if (clockLowPower) {
    if (wakeFlag) {
      // Button press or SQW: full wake
      wakeFlag = false;
      clockLowPower = false;
      wdt_disable();
      btnA = { BTN_SET,   false, false, 0, false };
      btnB = { BTN_START, false, false, 0, false };
      TinyWireM.begin();
      rtcClearAlarm();  // release SQW so PB4 reads HIGH
      oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
      oled.setFont(FONT_CHRONO);
      oled.on();
      lastActivity = millis();
      rtcRead(rtcHour, rtcMin, rtcSec);
      updateDisplay();
    } else {
      // WDT wake: update time only, sleep again
      TinyWireM.begin();
      rtcRead(rtcHour, rtcMin, rtcSec);
      oled.setCursor(0, 3);
      print2(rtcHour);
      oled.print(":");
      print2(rtcMin);
      oled.print(":");
      print2(rtcSec);
      clockSleep();
    }
    return;
  }

  if (wakeFlag) {
    wakeFlag = false;
    isSleeping = false;
    subState = SUB_IDLE;
    btnA = { BTN_SET,   false, false, 0, false };
    btnB = { BTN_START, false, false, 0, false };
    TinyWireM.begin();
    rtcClearAlarm();  // release SQW so PB4 reads HIGH
    oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
    oled.setFont(FONT_CHRONO);
    oled.on();
    lastActivity = millis();
    rtcRead(rtcHour, rtcMin, rtcSec);
    updateDisplay();
  }

  ButtonEvent evtA = readButton(btnA);
  ButtonEvent evtB = readButton(btnB);

  // Mode cycling (only from idle)
  if (evtA == EVT_LONG && subState == SUB_IDLE) {
    currentMode = (Mode)((currentMode + 1) % MODE_COUNT);
    subState = SUB_IDLE;
    targetSeconds = 0;
    currentSeconds = 0;
    swAccum = 0;
    swLapSecs = 0;
    swLapVisible = false;
    alarmFired = false;
    lastActivity = millis();
    beep();
    updateDisplay();
  }

  if (currentMode == MODE_TIMER) {
    if (subState == SUB_DONE) {
      // Any press dismisses alarm
      if (evtA != EVT_NONE || evtB != EVT_NONE) {
        subState = SUB_IDLE;
        lastActivity = millis();
        updateDisplay();
      }
    } else if (subState == SUB_RUNNING) {
      // Long B stops the timer
      if (evtB == EVT_LONG) {
        subState = SUB_IDLE;
        lastActivity = millis();
        updateDisplay();
      }
    } else {
      // IDLE or SETTING: adjust time
      if (evtA == EVT_SHORT) {
        if (targetSeconds < 5940) targetSeconds += 60; // cap 99 min
        subState = SUB_SETTING;
        lastActivity = millis();
        updateDisplay();
      }
      if (evtB == EVT_SHORT) {
        if (targetSeconds >= 60) targetSeconds -= 60;
        subState = SUB_SETTING;
        lastActivity = millis();
        updateDisplay();
      }
      // Long B starts the timer
      if (evtB == EVT_LONG && targetSeconds > 0) {
        currentSeconds = targetSeconds;
        subState = SUB_RUNNING;
        lastActivity = millis();
        beep();
        updateDisplay();
      }
    }

    // Timer countdown tick
    if (subState == SUB_RUNNING) {
      static uint32_t lastTick = 0;
      if (millis() - lastTick >= 1000) {
        lastTick = millis();
        if (currentSeconds > 0) {
          currentSeconds--;
          updateDisplay();
        } else {
          subState = SUB_DONE;
          beep();
          updateDisplay();
        }
      }
    }
  }

  if (currentMode == MODE_STOPWATCH) {
    if (evtB == EVT_SHORT) {
      if (subState == SUB_RUNNING) {
        swAccum += millis() - swStart;
        subState = SUB_IDLE;
      } else {
        swStart = millis();
        subState = SUB_RUNNING;
      }
      lastActivity = millis();
      updateDisplay();
    }

    if (evtA == EVT_SHORT) {
      if (subState == SUB_RUNNING) {
        uint32_t total = swAccum + (millis() - swStart);
        swLapSecs = (uint16_t)(total / 1000);
        swLapVisible = true;
      } else {
        swAccum = 0;
        swLapSecs = 0;
        swLapVisible = false;
      }
      lastActivity = millis();
      updateDisplay();
    }

    // Stopwatch display refresh (1Hz while running)
    if (subState == SUB_RUNNING) {
      static uint32_t lastSwRefresh = 0;
      if (millis() - lastSwRefresh >= 1000) {
        lastSwRefresh = millis();
        updateDisplay();
      }
    }
  }

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
      if (evtB == EVT_SHORT) {
        if (alarmEnabled) {
          alarmEnabled = false;
          rtcDisableAlarm();
        } else {
          settingHour = alarmHour;
          settingMin = alarmMin;
          settingField = 0;
          settingAlarm = true;
          subState = SUB_SETTING;
        }
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
    else if (subState == SUB_DONE) {
      if (evtA != EVT_NONE || evtB != EVT_NONE) {
        subState = SUB_IDLE;
        lastActivity = millis();
        rtcRead(rtcHour, rtcMin, rtcSec);
        updateDisplay();
      }
    }
  }

  // 1Hz RTC read (all modes); auto-refresh display in clock idle
  {
    static uint32_t lastRtcRead = 0;
    if (millis() - lastRtcRead >= 1000) {
      lastRtcRead = millis();
      rtcRead(rtcHour, rtcMin, rtcSec);
      if (currentMode == MODE_CLOCK && subState == SUB_IDLE) {
        updateDisplay();
      }
    }
  }

  // Clock alarm check
  if (currentMode == MODE_CLOCK && subState == SUB_IDLE && alarmEnabled) {
    if (rtcHour == alarmHour && rtcMin == alarmMin) {
      if (!alarmFired) {
        alarmFired = true;
        rtcClearAlarm();  // release SQW so PB4 reads HIGH
        subState = SUB_DONE;
        beep();
        updateDisplay();
      }
    } else {
      alarmFired = false;
    }
  }

  // Repeating alarm beep (timer done or clock alarm)
  if ((currentMode == MODE_TIMER || currentMode == MODE_CLOCK) && subState == SUB_DONE) {
    static uint32_t lastAlarmBeep = 0;
    if (millis() - lastAlarmBeep >= 2000) {
      beep();
      lastAlarmBeep = millis();
    }
  }

  // Auto-sleep after 15s inactivity (never during running/alarm/setting)
  if (millis() - lastActivity > 15000 &&
      subState != SUB_RUNNING &&
      subState != SUB_DONE &&
      subState != SUB_SETTING) {
    if (currentMode == MODE_CLOCK) {
      // Clock low-power: show only time, sleep between updates
      clockLowPower = true;
      oled.clear();
      if (alarmEnabled) {
        oled.setCursor(0, 0);
        oled.print("A");
      }
      oled.setCursor(0, 3);
      rtcRead(rtcHour, rtcMin, rtcSec);
      print2(rtcHour);
      oled.print(":");
      print2(rtcMin);
      oled.print(":");
      print2(rtcSec);
      oled.on();
      clockSleep();
    } else {
      isSleeping = true;
      goToSleep();
    }
  }
}
