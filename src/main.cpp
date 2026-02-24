#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <TinyWireM.h>
#include <Tiny4kOLED.h>

#define BTN_SET   PB3
#define BTN_START PB4
#define BUZZER    PB1

volatile bool wakeFlag = false;

enum State {
  SLEEPING,
  IDLE,
  SETTING,
  RUNNING,
  DONE
};

State state = IDLE;

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

  if (state == IDLE) {
    oled.print("Timer:");
  } else if (state == SETTING) {
    oled.print("Set:");
  } else if (state == RUNNING) {
    oled.print("Run:");
  } else if (state == DONE) {
    oled.print("Done!");
  }

  oled.setCursor(0, 2);
  uint16_t t = (state == RUNNING) ? currentSeconds : targetSeconds;
  oled.print(t / 60);
  oled.print(":");
  if ((t % 60) < 10) oled.print("0");
  oled.print(t % 60);

  oled.switchFrame();
}

void loop() {
  if (!wakeFlag && state == SLEEPING) {
    goToSleep();
  }

  if (wakeFlag) {
    wakeFlag = false;
    state = IDLE;
    TinyWireM.begin();
    oled.begin();
    oled.on();
    updateDisplay();
  }

  // Button handling
  bool setPressed = !digitalRead(BTN_SET);
  bool startPressed = !digitalRead(BTN_START);

  if (setPressed) {
    targetSeconds += 60;
    lastActivity = millis();
    state = SETTING;
    updateDisplay();
    delay(200);
  }

  if (startPressed) {
    if (state == RUNNING) {
      state = IDLE;
    } else {
      currentSeconds = targetSeconds;
      state = RUNNING;
    }
    lastActivity = millis();
    updateDisplay();
    delay(200);
  }

  // Timer logic
  if (state == RUNNING) {
    static uint32_t lastTick = 0;
    if (millis() - lastTick >= 1000) {
      lastTick = millis();
      if (currentSeconds > 0) {
        currentSeconds--;
        updateDisplay();
      } else {
        state = DONE;
        beep();
        updateDisplay();
      }
    }
  }

  // Auto-sleep after inactivity
  if (millis() - lastActivity > 15000 && state != RUNNING) {
    state = SLEEPING;
    goToSleep();
  }
}