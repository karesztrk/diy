/*
 *  This sketch demonstrates how to set up a simple HTTP-like server.
 *  The server will set a GPIO pin depending on the request
 *    http://server_ip/gpio/0 will set the GPIO2 low,
 *    http://server_ip/gpio/1 will set the GPIO2 high
 *  server_ip is the IP address of the ESP8266 module, will be 
 *  printed to Serial when the module is connected.
 */

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
const char* ssid = "Csokimaz";
const char* password = "alpha12345";

void setup() {
  pinMode(pin_led, OUTPUT);
  sensors.begin();
  
  WiFi.begin(ssid, password);
  Serial.begin(115200);
  delay(10);

  digitalWrite(pin_led, LOW);
  
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
}

void handleIndex() {
  float temperature = requestTemperature();
  server.send(200, "text/html" , String("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" charset=\"UTF-8\">")
    + String("<script>function toggle() {var xhttp = new XMLHttpRequest();xhttp.open(\"GET\", \"/toggle\", true);xhttp.send();}</script>")
    + String("<style>* { box-sizing: border-box; } .row::after { content: \"\"; clear: both; display: table; } [class*=\"col-\"] {float: left; padding: 15px;} .col-1 {width: 100%;} html {font-family: \"Lucida Sans\", sans-serif;} .header {background-color: #9933cc;color: #ffffff;padding: 15px;}</style></head>")
    + String("<body><div class=\"header\"><h1>ESP8266</h1></div>")
    + String("<div class=\"row\"><div class=\"col-1\"><h1>Hőmérséklet</h1><p>Akutális hőmérséklet " + String(temperature) + "&deg;C</p></div>")
    + String("<div class=\"row\"><div class=\"col-1\"><h1>LED</h1><button onclick=\"toggle()\">Kapcs</button></div>")
    + String("</body></html>"));
}

float requestTemperature() {
  sensors.setResolution(9);
  sensors.requestTemperatures(); // Send the command to get temperatures
  return sensors.getTempCByIndex(0);
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

