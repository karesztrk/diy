#include "HX711.h"

HX711 scale;

void setup() {
  Serial.begin(38400);

  Serial.println("Initializing the scale");
  scale.begin(D1, D2);
  scale.set_scale(20155.7);
  scale.tare();
  Serial.println("Scale initialized");
}

void loop() {
  Serial.print("Reading...");
  scale.get_units();
  Serial.println(scale.get_units(10), 1);

  scale.power_down();             // put the ADC in sleep mode
  delay(5000);
  scale.power_up();
}