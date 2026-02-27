#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <TinyWireM.h>
#include <Tiny4kOLED.h>

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

enum Mode { MODE_TIMER, MODE_STOPWATCH, MODE_COUNT };
enum SubState { SUB_IDLE, SUB_SETTING, SUB_RUNNING, SUB_DONE };

Mode currentMode = MODE_TIMER;
SubState subState = SUB_IDLE;
bool isSleeping = false;

uint16_t targetSeconds = 0;
uint16_t currentSeconds = 0;
uint32_t lastActivity = 0;

uint32_t swStart = 0;       // millis() when stopwatch started
uint32_t swAccum = 0;       // accumulated ms from previous runs
uint16_t swLapSecs = 0;     // last lap snapshot in seconds
bool swLapVisible = false;   // whether to show lap time

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

void setup() {
  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH);  // buzzer off (active-low)

  TinyWireM.begin();
  oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
  oled.setFont(FONT8X16);
  oled.clear();
  oled.on();
}

void beep() {
  digitalWrite(BUZZER, LOW);
  delay(150);
  digitalWrite(BUZZER, HIGH);
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

  // Sub-state indicator (right-aligned)
  oled.setCursor(88, 0);
  if (subState == SUB_SETTING) oled.print("SET");
  else if (subState == SUB_RUNNING) oled.print("RUN");
  else if (subState == SUB_DONE) oled.print("DONE");

  if (currentMode == MODE_TIMER) {
    oled.setCursor(0, 3);
    uint16_t t = (subState == SUB_RUNNING) ? currentSeconds : targetSeconds;
    uint8_t mins = t / 60;
    uint8_t secs = t % 60;
    if (mins < 10) oled.print("0");
    oled.print(mins);
    oled.print(":");
    if (secs < 10) oled.print("0");
    oled.print(secs);
  }

  if (currentMode == MODE_STOPWATCH) {
    uint32_t totalMs = swAccum;
    if (subState == SUB_RUNNING) {
      totalMs += millis() - swStart;
    }
    uint16_t t = (uint16_t)(totalMs / 1000);

    oled.setCursor(0, 3);
    uint8_t mins = t / 60;
    uint8_t secs = t % 60;
    if (mins < 10) oled.print("0");
    oled.print(mins);
    oled.print(":");
    if (secs < 10) oled.print("0");
    oled.print(secs);

    if (swLapVisible) {
      oled.setCursor(0, 6);
      oled.print("LAP ");
      uint8_t lm = swLapSecs / 60;
      uint8_t ls = swLapSecs % 60;
      if (lm < 10) oled.print("0");
      oled.print(lm);
      oled.print(":");
      if (ls < 10) oled.print("0");
      oled.print(ls);
    }
  }

  oled.on();
}

void loop() {
  if (!wakeFlag && isSleeping) {
    goToSleep();
  }

  if (wakeFlag) {
    wakeFlag = false;
    isSleeping = false;
    subState = SUB_IDLE;
    btnA = { BTN_SET,   false, false, 0, false };
    btnB = { BTN_START, false, false, 0, false };
    TinyWireM.begin();
    oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
    oled.setFont(FONT8X16);
    oled.on();
    lastActivity = millis();
    updateDisplay();
  }

  ButtonEvent evtA = readButton(btnA);
  ButtonEvent evtB = readButton(btnB);

  // Mode cycling (works in any mode)
  if (evtA == EVT_LONG) {
    currentMode = (Mode)((currentMode + 1) % MODE_COUNT);
    subState = SUB_IDLE;
    targetSeconds = 0;
    currentSeconds = 0;
    swAccum = 0;
    swLapSecs = 0;
    swLapVisible = false;
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

  // Repeating alarm when timer is done
  if (currentMode == MODE_TIMER && subState == SUB_DONE) {
    static uint32_t lastAlarmBeep = 0;
    if (millis() - lastAlarmBeep >= 2000) {
      beep();
      lastAlarmBeep = millis();
    }
  }

  // Auto-sleep after 15s inactivity (never during running/alarm)
  if (millis() - lastActivity > 15000 &&
      subState != SUB_RUNNING &&
      subState != SUB_DONE) {
    isSleeping = true;
    goToSleep();
  }
}
