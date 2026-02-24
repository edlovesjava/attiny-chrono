#include <TinyWireM.h>
#include <Tiny4kOLED.h>

uint16_t count = 0;

void setup() {
  TinyWireM.begin();
  oled.begin();
  oled.clear();
  oled.on();
  oled.setCursor(0, 0);
  oled.print("HELLO");
  oled.switchFrame();
}

void loop() {
  oled.clear();
  oled.setCursor(0, 0);
  oled.print("HELLO");
  oled.setCursor(0, 3);
  oled.print(count++);
  oled.switchFrame();
  delay(1000);
}
