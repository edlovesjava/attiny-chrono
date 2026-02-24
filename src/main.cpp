#include <Arduino.h>

void setup() {
  pinMode(PB1, OUTPUT);
}

void loop() {
  digitalWrite(PB1, HIGH);
  delay(500);
  digitalWrite(PB1, LOW);
  delay(500);
}
