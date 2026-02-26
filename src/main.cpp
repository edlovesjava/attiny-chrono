#include <TinyWireM.h>
#include <Tiny4kOLED.h>

void beep() {
  digitalWrite(PB1, LOW);
  delay(150);
  digitalWrite(PB1, HIGH);
}

void showStatus(const char* line1, const char* line2) {
  oled.clear();
  oled.setCursor(0, 0);
  oled.print(line1);
  oled.setCursor(0, 3);
  oled.print(line2);
  oled.on();
}

void setup() {
  pinMode(PB1, OUTPUT);
  digitalWrite(PB1, HIGH);  // buzzer off (active-low)
  pinMode(PB3, INPUT_PULLUP);
  pinMode(PB4, INPUT_PULLUP);

  TinyWireM.begin();
  oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
  oled.setFont(FONT8X16);
  showStatus("READY", "PRESS BTN");
  beep();
}

void loop() {
  if (!digitalRead(PB3)) {
    showStatus("BTN A", "PB3 pin2");
    beep();
    while (!digitalRead(PB3));
    delay(50);
    showStatus("READY", "PRESS BTN");
  }

  if (!digitalRead(PB4)) {
    showStatus("BTN B", "PB4 pin3");
    beep();
    while (!digitalRead(PB4));
    delay(50);
    showStatus("READY", "PRESS BTN");
  }
}
