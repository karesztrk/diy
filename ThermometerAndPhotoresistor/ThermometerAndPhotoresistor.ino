#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 5 // GPIO5

// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

ESP8266WebServer server;
int pin_led = 13; // GPIO13
int daylight_led = 4; // GPIO4

bool night = false;

const char* ssid = "Csokimaz";
const char* password = "alpha12345";

void setup() {
  pinMode(pin_led, OUTPUT);
  pinMode(daylight_led, OUTPUT);
  sensors.begin();

  WiFi.begin(ssid, password);
  Serial.begin(9600);
  delay(10);

  digitalWrite(pin_led, LOW);
  digitalWrite(daylight_led, LOW);
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  // Print the IP address
  Serial.println("IP address");
  Serial.println(WiFi.localIP());

  server.on("/", handleIndex);
  server.on("/toggle", toggleLED);
  server.begin();
}

void loop() {
  server.handleClient();
  float light = requestLight();
  if (light < 3 && !night) {
    toggleDayLightLED();
    night = true;
  } else if (light >= 3 && night) {
    toggleDayLightLED();
    night = false;
  }
  
  // Mandatory to add delay to handle Web clients
  delay(100);
}

void handleIndex() {
  float temperature = requestTemperature();
  server.send(200, "text/html" , String("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" charset=\"UTF-8\">")
    + String("<script>function toggle() {var xhttp = new XMLHttpRequest();xhttp.open(\"GET\", \"/toggle\", true);xhttp.send();}</script>")
    + String("<style>* { box-sizing: border-box; } .row::after { content: \"\"; clear: both; display: table; } [class*=\"col-\"] {float: left; padding: 15px;} .col-1 {width: 100%;} html {font-family: \"Lucida Sans\", sans-serif;} .header {background-color: #9933cc;color: #ffffff;padding: 15px;}</style></head>")
    + String("<body><div class=\"header\"><h1>ESP8266</h1></div>")
    + String("<div class=\"row\"><div class=\"col-1\"><h1>Hőmérséklet</h1><p>Akutális hőmérséklet " + String(temperature) + "&deg;C</p></div>")
    + String("<div class=\"row\"><div class=\"col-1\"><h1>Napszak</h1>Jelenleg ") + (night ? String("este") : String("nappal")) + String(" van</div>")
    + String("<div class=\"row\"><div class=\"col-1\"><h1>LED</h1><button onclick=\"toggle()\">Kapcs</button></div>")
    + String("</body></html>"));
}

float requestLight() {
  // read the input on analog pin 0:
  int sensorValue = analogRead(A0);
  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
  return sensorValue * (5.0 / 1023.0);
}

float requestTemperature() {
  sensors.setResolution(9);
  sensors.requestTemperatures(); // Send the command to get temperatures
  return sensors.getTempCByIndex(0);
}

void toggleDayLightLED() {
    digitalWrite(daylight_led, !digitalRead(daylight_led));
}

void toggleLED() {
  uint8_t state = digitalRead(pin_led);
  if (state) {
    digitalWrite(pin_led, LOW);
    server.send(200, "text/plain" , "Switched OFF");
  } else {
    digitalWrite(pin_led, HIGH);
    server.send(200, "text/plain" , "Switched ON");
  }
  
}

