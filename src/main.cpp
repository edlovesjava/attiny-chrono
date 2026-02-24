#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <TinyWireM.h>
#include <Tiny4kOLED.h>

#define BTN_SET   PB3
#define BTN_START PB4
#define BUZZER    PB1

#define DEBOUNCE_MS    50
#define LONG_PRESS_MS  600

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

void goToSleep() {
  oled.off();          // OLED sleep command
  oled.clear();
  oled.switchFrame();

  GIMSK |= _BV(PCIE);  // Enable pin change interrupts
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

  TinyWireM.begin();
  oled.begin();
  oled.clear();
  oled.on();
}

void beep() {
  digitalWrite(BUZZER, HIGH);
  delay(150);
  digitalWrite(BUZZER, LOW);
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

  // Time display (timer mode only for now)
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

  oled.switchFrame();
}

void loop() {
  if (!wakeFlag && isSleeping) {
    goToSleep();
  }

  if (wakeFlag) {
    wakeFlag = false;
    isSleeping = false;
    subState = SUB_IDLE;
    // Reset button state so wake press doesn't trigger action
    btnA = { BTN_SET,   false, false, 0, false };
    btnB = { BTN_START, false, false, 0, false };
    TinyWireM.begin();
    oled.begin();
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
    lastActivity = millis();
    beep();
    updateDisplay();
  }

  if (currentMode == MODE_TIMER) {
    if (evtA == EVT_SHORT) {
      targetSeconds += 60;
      lastActivity = millis();
      subState = SUB_SETTING;
      updateDisplay();
    }

    if (evtB == EVT_SHORT) {
      if (subState == SUB_RUNNING) {
        subState = SUB_IDLE;
      } else {
        currentSeconds = targetSeconds;
        subState = SUB_RUNNING;
      }
      lastActivity = millis();
      updateDisplay();
    }

    // Timer logic
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

  // Auto-sleep after inactivity
  if (millis() - lastActivity > 15000 && subState != SUB_RUNNING) {
    isSleeping = true;
    goToSleep();
  }
}