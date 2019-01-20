#include "HX711.h"

// Scale Settings
const int SCALE_DOUT_PIN = D1;
const int SCALE_SCK_PIN = D2;
const float scaleValue = 20155.7;

HX711 scale;

bool measuring = false;
bool debug = true;

void setup() {
  Serial.begin(38400);

  Serial.println("Initializing the scale");
  scale.begin(SCALE_DOUT_PIN, SCALE_SCK_PIN);
  scale.set_scale(scaleValue);
  scale.tare();
  Serial.println("Scale initialized");
}

void loop() {

  float currentWeight = measureWeight();

  if (isBodyWeight(currentWeight)) {

    if (!measuring) {
      switchMeasuring(true);

      if (debug) {
        Serial.print("New weight...");
        Serial.println(currentWeight, 1);  
      }
    }
 
  } else {
    switchMeasuring(false);
  }

  scale.power_down();
  delay(5000);
  scale.power_up();
}

float measureWeight() {
  if (debug) {
    Serial.print("Reading scale...");
  }
  scale.get_units();
  float weight = scale.get_units(10);
  if (debug) {
    Serial.println("Done");
  }
  return weight;
}

bool isBodyWeight(float weight) {
  return weight > 1.0;
}

void switchMeasuring(bool state) {
  measuring = state;
}

